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

#include "core/operation/detail/task.h"

namespace legate::detail {

inline Task::ArrayArg::ArrayArg(InternalSharedPtr<LogicalArray> _array) : array{std::move(_array)}
{
}

inline Task::ArrayArg::ArrayArg(InternalSharedPtr<LogicalArray> _array,
                                std::optional<SymbolicPoint> _projection)
  : array{std::move(_array)}, projection{std::move(_projection)}
{
}

// ==========================================================================================

inline bool Task::always_flush() const { return can_throw_exception_; }

inline bool Task::supports_replicated_write() const { return true; }

// ==========================================================================================

inline AutoTask::AutoTask(const Library* library,
                          std::int64_t task_id,
                          std::uint64_t unique_id,
                          std::int32_t priority,
                          mapping::detail::Machine machine)
  : Task{library, task_id, unique_id, priority, std::move(machine)}
{
}

inline Operation::Kind AutoTask::kind() const { return Kind::AUTO_TASK; }

// ==========================================================================================

inline void ManualTask::validate() {}

inline void ManualTask::launch(Strategy* /*strategy*/) { launch(); }

inline void ManualTask::launch() { launch_task_(strategy_.get()); }

inline void ManualTask::add_to_solver(ConstraintSolver& /*solver*/) {}

inline Operation::Kind ManualTask::kind() const { return Kind::MANUAL_TASK; }

}  // namespace legate::detail
