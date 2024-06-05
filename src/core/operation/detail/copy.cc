/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "core/operation/detail/copy.h"

#include "core/operation/detail/copy_launcher.h"
#include "core/partitioning/detail/constraint.h"
#include "core/partitioning/detail/constraint_solver.h"
#include "core/partitioning/detail/partitioner.h"
#include "core/partitioning/partition.h"
#include "core/type/detail/type_info.h"

namespace legate::detail {

Copy::Copy(InternalSharedPtr<LogicalStore> target,
           InternalSharedPtr<LogicalStore> source,
           std::uint64_t unique_id,
           std::int32_t priority,
           mapping::detail::Machine machine,
           std::optional<std::int32_t> redop)
  : Operation{unique_id, priority, std::move(machine)},
    target_{target, declare_partition()},
    source_{source, declare_partition()},
    constraint_{align(target_.variable, source_.variable)},
    redop_{redop}
{
  record_partition_(target_.variable, std::move(target));
  record_partition_(source_.variable, std::move(source));
}

void Copy::validate()
{
  if (*source_.store->type() != *target_.store->type()) {
    throw std::invalid_argument("Source and target must have the same type");
  }
  auto validate_store = [](const auto& store) {
    if (store->unbound() || store->transformed()) {
      throw std::invalid_argument("Copy accepts only normal and untransformed stores");
    }
  };
  validate_store(target_.store);
  validate_store(source_.store);
  constraint_->validate();

  if (target_.store->has_scalar_storage() != source_.store->has_scalar_storage()) {
    throw std::runtime_error("Copies are supported only between the same kind of stores");
  }
  if (redop_ && target_.store->has_scalar_storage()) {
    throw std::runtime_error("Reduction copies don't support future-backed target stores");
  }
}

void Copy::launch(Strategy* p_strategy)
{
  if (target_.store->has_scalar_storage()) {
    LEGATE_CHECK(source_.store->has_scalar_storage());
    target_.store->set_future(source_.store->get_future());
    return;
  }
  auto& strategy     = *p_strategy;
  auto launcher      = CopyLauncher{machine_, priority()};
  auto launch_domain = strategy.launch_domain(this);

  launcher.add_input(source_.store, create_store_projection_(strategy, launch_domain, source_));

  if (!redop_) {
    launcher.add_output(target_.store, create_store_projection_(strategy, launch_domain, target_));
  } else {
    auto store_partition = create_store_partition(target_.store, strategy[target_.variable]);
    auto proj            = store_partition->create_store_projection(launch_domain);

    proj->set_reduction_op(static_cast<Legion::ReductionOpID>(
      target_.store->type()->find_reduction_operator(redop_.value())));
    launcher.add_reduction(target_.store, std::move(proj));
  }

  if (launch_domain.is_valid()) {
    launcher.execute(launch_domain);
    return;
  }
  launcher.execute_single();
}

void Copy::add_to_solver(ConstraintSolver& solver)
{
  solver.add_constraint(std::move(constraint_));
  solver.add_partition_symbol(target_.variable, !redop_ ? AccessMode::WRITE : AccessMode::REDUCE);
  if (target_.store->has_scalar_storage()) {
    solver.add_constraint(broadcast(target_.variable));
  }
  solver.add_partition_symbol(source_.variable, AccessMode::READ);
}

}  // namespace legate::detail
