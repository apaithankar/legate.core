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

#include "core/operation/detail/gather.h"

#include "core/operation/detail/copy_launcher.h"
#include "core/partitioning/detail/constraint.h"
#include "core/partitioning/detail/constraint_solver.h"
#include "core/partitioning/detail/partitioner.h"
#include "core/partitioning/partition.h"

namespace legate::detail {

Gather::Gather(InternalSharedPtr<LogicalStore> target,
               InternalSharedPtr<LogicalStore> source,
               InternalSharedPtr<LogicalStore> source_indirect,
               std::uint64_t unique_id,
               std::int32_t priority,
               mapping::detail::Machine machine,
               std::optional<std::int32_t> redop_kind)
  : Operation{unique_id, priority, std::move(machine)},
    target_{target, declare_partition()},
    source_{source, declare_partition()},
    source_indirect_{source_indirect, declare_partition()},
    constraint_(align(target_.variable, source_indirect_.variable)),
    redop_kind_{redop_kind}
{
  record_partition_(target_.variable, std::move(target));
  record_partition_(source_.variable, std::move(source));
  record_partition_(source_indirect_.variable, std::move(source_indirect));
}

void Gather::validate()
{
  if (*source_.store->type() != *target_.store->type()) {
    throw std::invalid_argument("Source and targets must have the same type");
  }
  auto validate_store = [](const auto& store) {
    if (store->unbound() || store->has_scalar_storage() || store->transformed()) {
      throw std::invalid_argument(
        "Gather accepts only normal, untransformed, region-backed stores");
    }
  };
  validate_store(target_.store);
  validate_store(source_.store);
  validate_store(source_indirect_.store);

  if (!is_point_type(source_indirect_.store->type(), source_.store->dim())) {
    throw std::invalid_argument("Indirection store should contain " +
                                std::to_string(source_.store->dim()) + "-D points");
  }

  constraint_->validate();
}

void Gather::launch(Strategy* p_strategy)
{
  auto& strategy       = *p_strategy;
  auto launcher        = CopyLauncher{machine_, priority()};
  auto&& launch_domain = strategy.launch_domain(this);

  launcher.add_input(source_.store, create_store_projection_(strategy, launch_domain, source_));
  launcher.add_source_indirect(source_indirect_.store,
                               create_store_projection_(strategy, launch_domain, source_indirect_));

  if (!redop_kind_) {
    launcher.add_output(target_.store, create_store_projection_(strategy, launch_domain, target_));
  } else {
    auto store_partition = create_store_partition(target_.store, strategy[target_.variable]);
    auto proj            = store_partition->create_store_projection(launch_domain);

    proj->set_reduction_op(target_.store->type()->find_reduction_operator(redop_kind_.value()));
    launcher.add_reduction(target_.store, std::move(proj));
  }

  launcher.set_source_indirect_out_of_range(out_of_range_);

  if (launch_domain.is_valid()) {
    launcher.execute(launch_domain);
  } else {
    launcher.execute_single();
  }
}

void Gather::add_to_solver(ConstraintSolver& solver)
{
  solver.add_constraint(std::move(constraint_));
  solver.add_partition_symbol(target_.variable,
                              !redop_kind_ ? AccessMode::WRITE : AccessMode::REDUCE);
  solver.add_partition_symbol(source_.variable, AccessMode::READ);
  solver.add_partition_symbol(source_indirect_.variable, AccessMode::READ);
}

}  // namespace legate::detail
