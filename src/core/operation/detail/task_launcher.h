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

#pragma once

#include <memory>

#include "legion.h"

#include "core/data/detail/scalar.h"
#include "core/mapping/detail/machine.h"
#include "core/operation/detail/launcher_arg.h"
#include "core/operation/detail/req_analyzer.h"
#include "core/utilities/memory.h"

namespace legate::detail {

class Library;
class LogicalStore;
struct ProjectionInfo;

class TaskLauncher {
 public:
  TaskLauncher(const Library* library,
               const mapping::detail::Machine& machine,
               int64_t task_id,
               int64_t tag = 0);
  TaskLauncher(const Library* library,
               const mapping::detail::Machine& machine,
               const std::string& provenance,
               int64_t task_id,
               int64_t tag = 0);
  ~TaskLauncher();

 public:
  int64_t legion_task_id() const;
  int64_t legion_mapper_id() const;

 public:
  void add_input(std::unique_ptr<Analyzable> arg);
  void add_output(std::unique_ptr<Analyzable> arg);
  void add_reduction(std::unique_ptr<Analyzable> arg);
  void add_scalar(Scalar&& scalar);

 public:
  void add_future(const Legion::Future& future);
  void add_future_map(const Legion::FutureMap& future_map);
  void add_communicator(const Legion::FutureMap& communicator);

 public:
  void set_side_effect(bool has_side_effect) { has_side_effect_ = has_side_effect; }
  void set_concurrent(bool is_concurrent) { concurrent_ = is_concurrent; }
  void set_insert_barrier(bool insert_barrier) { insert_barrier_ = insert_barrier; }
  void throws_exception(bool can_throw_exception) { can_throw_exception_ = can_throw_exception; }

 public:
  Legion::FutureMap execute(const Legion::Domain& launch_domain);
  Legion::Future execute_single();

 private:
  void pack_mapper_arg(BufferBuilder& buffer);
  void post_process_unbound_stores(
    const std::vector<Legion::OutputRequirement>& output_requirements);
  void post_process_unbound_stores(
    const Legion::FutureMap& result,
    const Legion::Domain& launch_domain,
    const std::vector<Legion::OutputRequirement>& output_requirements);

 private:
  const Library* library_;
  int64_t task_id_;
  int64_t tag_;
  const mapping::detail::Machine& machine_;
  std::string provenance_;

 private:
  bool has_side_effect_{true};
  bool concurrent_{false};
  bool insert_barrier_{false};
  bool can_throw_exception_{false};

 private:
  std::vector<std::unique_ptr<Analyzable>> inputs_;
  std::vector<std::unique_ptr<Analyzable>> outputs_;
  std::vector<std::unique_ptr<Analyzable>> reductions_;
  std::vector<std::unique_ptr<ScalarArg>> scalars_;
  std::vector<Legion::Future> futures_;
  std::vector<const OutputRegionArg*> unbound_stores_;
  std::vector<Legion::FutureMap> future_maps_;
  std::vector<Legion::FutureMap> communicators_;
};

}  // namespace legate::detail
