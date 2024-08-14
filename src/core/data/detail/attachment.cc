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

#include "core/data/detail/attachment.h"

#include "legate_defines.h"

#include "core/runtime/detail/runtime.h"

namespace legate::detail {

Legion::Future SingleAttachment::detach(bool unordered) const
{
  LEGATE_ASSERT(allocation_);
  LEGATE_ASSERT(physical_region_);
  return Runtime::get_runtime()->detach(*physical_region_, !allocation_->read_only(), unordered);
}

void SingleAttachment::maybe_deallocate() noexcept
{
  if (!allocation_) {
    return;
  }
  allocation_->maybe_deallocate();
  allocation_.reset();
}

// ==========================================================================================

// Leak is intentional
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
IndexAttachment::~IndexAttachment()
{
  maybe_deallocate();
  // FIXME: Leak the ExternalResources handle if the runtime has already shut down, as
  // there's no hope that this would be collected by the Legion runtime
  if (!has_started() && external_resources_.exists()) {
    static_cast<void>(std::make_unique<Legion::ExternalResources>(std::move(external_resources_))
                        .release());  // NOLINT(bugprone-unused-return-value)
  }
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

Legion::Future IndexAttachment::detach(bool unordered) const
{
  // Index attachments are always read-only, so no need to flush
  return Runtime::get_runtime()->detach(external_resources_, false /*flush*/, unordered);
}

void IndexAttachment::maybe_deallocate() noexcept
{
  for (auto&& allocation : allocations_) {
    allocation->maybe_deallocate();
  }
  allocations_.clear();
}

}  // namespace legate::detail
