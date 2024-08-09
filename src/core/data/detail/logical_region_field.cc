/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "core/data/detail/logical_region_field.h"

#include "core/partitioning/detail/partition.h"
#include "core/runtime/detail/field_manager.h"
#include "core/runtime/detail/runtime.h"
#include "core/utilities/detail/tuple.h"

namespace legate::detail {

namespace {

// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void intentionally_leak_physical_region(Legion::PhysicalRegion pr)
{
  if (pr.exists()) {
    static_cast<void>(std::make_unique<Legion::PhysicalRegion>(std::move(pr))
                        .release());  // NOLINT(bugprone-unused-return-value)
  }
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

}  // namespace

LogicalRegionField::~LogicalRegionField()
{
  // Only free associated resources when the top-level region is deleted.
  if (parent_ == nullptr) {
    // If the runtime is already destroyed, no need to clean up the resource
    if (!has_started()) {
      intentionally_leak_physical_region(std::move(pr_));
      return;
    }

    perform_invalidation_callbacks();

    // We unmap the field immediately. In the case where a LogicalStore is allowed to be destroyed
    // out-of-order, this unmapping might happen at different times on different shards. Unmapping
    // doesn't go through the Legion pipeline, so from that perspective it's not critical that all
    // shards unmap a region in the same order. The only problematic case is when shard A unmaps
    // region R and shard B doesn't, then both shards launch a task that uses R (or any region
    // that overlaps with R). Then B will unmap/remap around the task, whereas A will not. This
    // shouldn't be an issue in Legate, because once a shard has (locally) freed a root
    // RegionField, there should be no Stores remaining that use it (or any of its sub-regions).
    // Moreover, the field will only start to get reused once all shards have agreed that it's
    // been collected.
    auto* runtime = Runtime::get_runtime();

    if (pr_.is_mapped()) {
      runtime->unmap_physical_region(pr_);
    }
    auto can_dealloc = attachment_ ? attachment_->detach(destroyed_out_of_order_ /*unordered*/)
                                   : Legion::Future();  // waiting on this is a noop
    runtime->field_manager()->free_field(
      FreeFieldInfo{shape_, field_size_, lr_, fid_, std::move(can_dealloc), std::move(attachment_)},
      destroyed_out_of_order_);
  }
}

std::int32_t LogicalRegionField::dim() const { return lr_.get_dim(); }

const LogicalRegionField& LogicalRegionField::get_root() const
{
  return parent_ ? parent_->get_root() : *this;
}

Domain LogicalRegionField::domain() const
{
  return Runtime::get_runtime()->get_index_space_domain(lr_.get_index_space());
}

bool LogicalRegionField::is_mapped() const
{
  // Only the root has a physical region at the moment
  if (parent()) {
    return parent()->is_mapped();
  }
  return pr_.exists();
}

RegionField LogicalRegionField::map()
{
  if (parent_ != nullptr) {
    LEGATE_ASSERT(!pr_.exists());
    return parent_->map();
  }
  if (!pr_.exists()) {
    pr_ = Runtime::get_runtime()->map_region_field(lr_, fid_);
  } else if (!pr_.is_mapped()) {
    Runtime::get_runtime()->remap_physical_region(pr_);
  }
  return {dim(), pr_, fid_};
}

void LogicalRegionField::attach(Legion::PhysicalRegion physical_region,
                                InternalSharedPtr<ExternalAllocation> allocation)
{
  LEGATE_ASSERT(!parent_);
  LEGATE_ASSERT(physical_region.exists());
  LEGATE_ASSERT(!attachment_ && !pr_.exists());
  pr_         = std::move(physical_region);
  attachment_ = std::make_unique<SingleAttachment>(&pr_, std::move(allocation));
}

void LogicalRegionField::attach(const Legion::ExternalResources& external_resources,
                                std::vector<InternalSharedPtr<ExternalAllocation>> allocations)
{
  LEGATE_ASSERT(!parent_);
  LEGATE_ASSERT(external_resources.exists());
  LEGATE_ASSERT(!attachment_);
  attachment_ = std::make_unique<IndexAttachment>(external_resources, std::move(allocations));
}

void LogicalRegionField::detach()
{
  auto* runtime = Runtime::get_runtime();
  if (!runtime->initialized()) {
    intentionally_leak_physical_region(std::move(pr_));
    return;
  }
  if (nullptr != parent_) {
    throw std::invalid_argument{"Manual detach must be called on the root store"};
  }
  if (!attachment_) {
    throw std::invalid_argument{"Store has no attachment to detach"};
  }
  LEGATE_CHECK(pr_.exists());
  if (pr_.is_mapped()) {
    runtime->unmap_physical_region(pr_);
  }
  auto fut = attachment_->detach(false /*unordered*/);
  fut.get_void_result(true /*silence_warnings*/);
  attachment_->maybe_deallocate();

  // Reset the object
  attachment_.reset();
  pr_ = Legion::PhysicalRegion{};
}

void LogicalRegionField::allow_out_of_order_destruction()
{
  if (parent_) {
    parent_->allow_out_of_order_destruction();
  } else {
    destroyed_out_of_order_ = true;
  }
}

InternalSharedPtr<LogicalRegionField> LogicalRegionField::get_child(
  const Tiling* tiling, const tuple<std::uint64_t>& color, bool complete)
{
  auto legion_partition = get_legion_partition(tiling, complete);
  auto color_point      = to_domain_point(color);
  return make_internal_shared<LogicalRegionField>(
    shape_,
    field_size_,
    Runtime::get_runtime()->get_subregion(std::move(legion_partition), color_point),
    fid_,
    shared_from_this());
}

Legion::LogicalPartition LogicalRegionField::get_legion_partition(const Partition* partition,
                                                                  bool complete)
{
  return partition->construct(lr_, complete);
}

void LogicalRegionField::add_invalidation_callback_(std::function<void()> callback)
{
  if (parent_) {
    parent_->add_invalidation_callback_(std::move(callback));
  } else {
    callbacks_.emplace_back(std::move(callback));
  }
}

// This clang-tidy error is spurious. All of the callbacks are noexcept (they are checked as
// such during registration) but the compiler doesn't know that, since std::function does not
// (as of C++23) allow you to specify the exception specification. Otherwise we would make
// callbacks_ a container of std::function<void() noexcept>.
// NOLINTNEXTLINE(bugprone-exception-escape)
void LogicalRegionField::perform_invalidation_callbacks() noexcept
{
  if (parent_) {
    // Callbacks should exist only in the root
    LEGATE_ASSERT(callbacks_.empty());
    parent_->perform_invalidation_callbacks();
  } else {
    for (auto&& callback : callbacks_) {
      callback();
    }
    callbacks_.clear();
  }
}

}  // namespace legate::detail
