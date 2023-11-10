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

Copy::Copy(std::shared_ptr<LogicalStore> target,
           std::shared_ptr<LogicalStore> source,
           uint64_t unique_id,
           mapping::detail::Machine&& machine,
           std::optional<int32_t> redop)
  : Operation{unique_id, std::move(machine)},
    target_{target.get(), declare_partition()},
    source_{source.get(), declare_partition()},
    constraint_{align(target_.variable, source_.variable)},
    redop_{redop}
{
  record_partition(target_.variable, std::move(target));
  record_partition(source_.variable, std::move(source));
}

void Copy::validate()
{
  if (*source_.store->type() != *target_.store->type()) {
    throw std::invalid_argument("Source and target must have the same type");
  }
  auto validate_store = [](auto* store) {
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
    assert(source_.store->has_scalar_storage());
    target_.store->set_future(source_.store->get_future());
    return;
  }
  auto& strategy     = *p_strategy;
  auto launcher      = CopyLauncher{machine_};
  auto launch_domain = strategy.launch_domain(this);

  launcher.add_input(source_.store, create_projection_info(strategy, launch_domain, source_));

  if (!redop_) {
    launcher.add_output(target_.store, create_projection_info(strategy, launch_domain, target_));
  } else {
    auto store_partition = target_.store->create_partition(strategy[target_.variable]);
    auto proj            = store_partition->create_projection_info(launch_domain);

    proj->set_reduction_op(static_cast<Legion::ReductionOpID>(
      target_.store->type()->find_reduction_operator(redop_.value())));
    launcher.add_reduction(target_.store, std::move(proj));
  }

  if (launch_domain.is_valid()) return launcher.execute(launch_domain);
  return launcher.execute_single();
}

void Copy::add_to_solver(ConstraintSolver& solver)
{
  solver.add_constraint(std::move(constraint_));
  solver.add_partition_symbol(target_.variable, IsOutput::Y);
  if (target_.store->has_scalar_storage()) solver.add_constraint(broadcast(target_.variable));
  solver.add_partition_symbol(source_.variable, IsOutput::N);
}

std::string Copy::to_string() const { return "Copy:" + std::to_string(unique_id_); }

}  // namespace legate::detail
