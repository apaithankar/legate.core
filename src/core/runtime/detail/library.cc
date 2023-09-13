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

#include "core/runtime/detail/library.h"

#include "core/mapping/detail/base_mapper.h"
#include "core/mapping/machine.h"
#include "core/mapping/mapping.h"
#include "core/runtime/runtime.h"

#include "mappers/logging_wrapper.h"

namespace legate::detail {

Library::Library(const std::string& library_name,
                 const ResourceConfig& config,
                 std::unique_ptr<mapping::Mapper> mapper)
  : runtime_(Legion::Runtime::get_runtime()), library_name_(library_name)
{
  task_scope_ = ResourceIdScope(
    runtime_->generate_library_task_ids(library_name.c_str(), config.max_tasks), config.max_tasks);
  redop_scope_ = ResourceIdScope(
    runtime_->generate_library_reduction_ids(library_name.c_str(), config.max_reduction_ops),
    config.max_reduction_ops);
  proj_scope_ = ResourceIdScope(
    runtime_->generate_library_projection_ids(library_name.c_str(), config.max_projections),
    config.max_projections);
  shard_scope_ = ResourceIdScope(
    runtime_->generate_library_sharding_ids(library_name.c_str(), config.max_shardings),
    config.max_shardings);
  mapper_id_ = runtime_->generate_library_mapper_ids(library_name.c_str(), 1);

  register_mapper(std::move(mapper));
}

const std::string& Library::get_library_name() const { return library_name_; }

Legion::TaskID Library::get_task_id(int64_t local_task_id) const
{
  assert(task_scope_.valid());
  return task_scope_.translate(local_task_id);
}

Legion::ReductionOpID Library::get_reduction_op_id(int64_t local_redop_id) const
{
  assert(redop_scope_.valid());
  return redop_scope_.translate(local_redop_id);
}

Legion::ProjectionID Library::get_projection_id(int64_t local_proj_id) const
{
  if (local_proj_id == 0)
    return 0;
  else {
    assert(proj_scope_.valid());
    return proj_scope_.translate(local_proj_id);
  }
}

Legion::ShardingID Library::get_sharding_id(int64_t local_shard_id) const
{
  assert(shard_scope_.valid());
  return shard_scope_.translate(local_shard_id);
}

int64_t Library::get_local_task_id(Legion::TaskID task_id) const
{
  assert(task_scope_.valid());
  return task_scope_.invert(task_id);
}

int64_t Library::get_local_reduction_op_id(Legion::ReductionOpID redop_id) const
{
  assert(redop_scope_.valid());
  return redop_scope_.invert(redop_id);
}

int64_t Library::get_local_projection_id(Legion::ProjectionID proj_id) const
{
  if (proj_id == 0)
    return 0;
  else {
    assert(proj_scope_.valid());
    return proj_scope_.invert(proj_id);
  }
}

int64_t Library::get_local_sharding_id(Legion::ShardingID shard_id) const
{
  assert(shard_scope_.valid());
  return shard_scope_.invert(shard_id);
}

bool Library::valid_task_id(Legion::TaskID task_id) const { return task_scope_.in_scope(task_id); }

bool Library::valid_reduction_op_id(Legion::ReductionOpID redop_id) const
{
  return redop_scope_.in_scope(redop_id);
}

bool Library::valid_projection_id(Legion::ProjectionID proj_id) const
{
  return proj_scope_.in_scope(proj_id);
}

bool Library::valid_sharding_id(Legion::ShardingID shard_id) const
{
  return shard_scope_.in_scope(shard_id);
}

const std::string& Library::get_task_name(int64_t local_task_id) const
{
  return find_task(local_task_id)->name();
}

void Library::register_mapper(std::unique_ptr<mapping::Mapper> mapper)
{
  // Hold the pointer to the mapper to keep it alive
  mapper_ = std::move(mapper);

  auto base_mapper =
    new mapping::detail::BaseMapper(mapper_.get(), runtime_->get_mapper_runtime(), this);
  Legion::Mapping::Mapper* legion_mapper = base_mapper;
  if (Core::log_mapping_decisions)
    legion_mapper = new Legion::Mapping::LoggingWrapper(base_mapper, &base_mapper->logger);
  runtime_->add_mapper(mapper_id_, legion_mapper);
}

void Library::register_task(int64_t local_task_id, std::unique_ptr<TaskInfo> task_info)
{
  auto task_id = get_task_id(local_task_id);
  if (!task_scope_.in_scope(task_id)) {
    std::stringstream ss;
    ss << "Task " << local_task_id << " is invalid for library '" << library_name_
       << "' (max local task id: " << (task_scope_.size() - 1) << ")";
    throw std::out_of_range(std::move(ss).str());
  }

  if (LegateDefined(LEGATE_USE_DEBUG)) {
    log_legate.debug() << "[" << library_name_ << "] task " << local_task_id
                       << " (global id: " << task_id << "), " << *task_info;
  }
  if (tasks_.find(local_task_id) != tasks_.end())
    throw std::invalid_argument("Task " + std::to_string(local_task_id) +
                                " already exists in library " + library_name_);
  task_info->register_task(task_id);
  tasks_.emplace(std::make_pair(local_task_id, std::move(task_info)));
}

const TaskInfo* Library::find_task(int64_t local_task_id) const
{
  auto finder = tasks_.find(local_task_id);
  if (tasks_.end() == finder) {
    throw std::out_of_range("Library " + get_library_name() + " does not have task " +
                            std::to_string(local_task_id));
  }
  return finder->second.get();
}

}  // namespace legate::detail
