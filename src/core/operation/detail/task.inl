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

#pragma once

#include "core/operation/detail/task.h"
#include "core/runtime/detail/config.h"

namespace legate::detail {

inline Task::ArrayArg::ArrayArg(InternalSharedPtr<LogicalArray> _array) : array{std::move(_array)}
{
}

inline Task::ArrayArg::ArrayArg(InternalSharedPtr<LogicalArray> _array,
                                std::optional<SymbolicPoint> _projection)
  : array{std::move(_array)}, projection{std::move(_projection)}
{
}

inline bool Task::ArrayArg::needs_flush() const { return array->needs_flush(); }

// ==========================================================================================

inline bool Task::supports_replicated_write() const { return true; }

inline bool Task::can_throw_exception() const { return can_throw_exception_; }

inline bool Task::can_elide_device_ctx_sync() const { return can_elide_device_ctx_sync_; }

inline const Library* Task::library() const { return library_; }

inline LocalTaskID Task::local_task_id() const { return task_id_; }

inline const std::vector<InternalSharedPtr<Scalar>>& Task::scalars() const { return scalars_; }

inline const std::vector<Task::ArrayArg>& Task::inputs() const { return inputs_; }

inline const std::vector<Task::ArrayArg>& Task::outputs() const { return outputs_; }

inline const std::vector<Task::ArrayArg>& Task::reductions() const { return reductions_; }

inline const std::vector<InternalSharedPtr<LogicalStore>>& Task::scalar_outputs() const
{
  return scalar_outputs_;
}

inline const std::vector<std::pair<InternalSharedPtr<LogicalStore>, GlobalRedopID>>&
Task::scalar_reductions() const
{
  return scalar_reductions_;
}

// ==========================================================================================

inline AutoTask::AutoTask(const Library* library,
                          LocalTaskID task_id,
                          std::uint64_t unique_id,
                          std::int32_t priority,
                          mapping::detail::Machine machine)
  : Task{library,
         task_id,
         unique_id,
         priority,
         std::move(machine),
         /* can_inline_launch */ Config::enable_inline_task_launch}
{
}

inline Operation::Kind AutoTask::kind() const { return Kind::AUTO_TASK; }

// ==========================================================================================

// TODO(wonchanl): Needs to validate interfering store accesses in this method
inline void ManualTask::validate() {}

inline void ManualTask::launch() { launch_task_(strategy_.get()); }

inline Operation::Kind ManualTask::kind() const { return Kind::MANUAL_TASK; }

}  // namespace legate::detail
