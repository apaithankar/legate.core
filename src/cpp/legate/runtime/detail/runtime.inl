/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved. SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

// Useful for IDEs
#include <legate/runtime/detail/runtime.h>

namespace legate::detail {

template <typename T>
ConsensusMatchResult<T> Runtime::issue_consensus_match(std::vector<T>&& input)
{
  return {std::move(input), legion_context_, legion_runtime_};
}

inline bool Runtime::initialized() const { return initialized_; }

inline void Runtime::register_shutdown_callback(ShutdownCallback callback)
{
  callbacks_.emplace_back(std::move(callback));
}

inline const Library* Runtime::core_library() const { return core_library_; }

inline Legion::Runtime* Runtime::get_legion_runtime() { return legion_runtime_; }

inline Legion::Context Runtime::get_legion_context() { return legion_context_; }

inline std::uint64_t Runtime::current_op_id_() const { return cur_op_id_; }

inline void Runtime::increment_op_id_() { ++cur_op_id_; }

inline std::uint64_t Runtime::get_unique_store_id() { return next_store_id_++; }

inline std::uint64_t Runtime::get_unique_storage_id() { return next_storage_id_++; }

inline std::uint32_t Runtime::field_reuse_freq() const { return field_reuse_freq_; }

inline std::size_t Runtime::field_reuse_size() const { return field_reuse_size_; }

inline FieldManager* Runtime::field_manager() { return field_manager_.get(); }

inline PartitionManager* Runtime::partition_manager()
{
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    return &partition_manager_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  return &*partition_manager_;  // NOLINT(bugprone-unchecked-optional-access)
}

inline const PartitionManager* Runtime::partition_manager() const
{
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    return &partition_manager_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  return &*partition_manager_;  // NOLINT(bugprone-unchecked-optional-access)
}

inline CommunicatorManager* Runtime::communicator_manager()
{
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    return &communicator_manager_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  return &*communicator_manager_;  // NOLINT(bugprone-unchecked-optional-access)
}

inline const CommunicatorManager* Runtime::communicator_manager() const
{
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    return &communicator_manager_.value();  // NOLINT(bugprone-unchecked-optional-access)
  }
  return &*communicator_manager_;  // NOLINT(bugprone-unchecked-optional-access)
}

inline Scope& Runtime::scope() { return scope_; }

inline const Scope& Runtime::scope() const { return scope_; }

inline const mapping::detail::LocalMachine& Runtime::local_machine() const
{
  return local_machine_;
}

inline std::uint32_t Runtime::node_count() const { return local_machine().total_nodes; }

inline std::uint32_t Runtime::node_id() const { return local_machine().node_id; }

inline Processor Runtime::get_executing_processor() const
{
  // Cannot use member legion_context_ here since we may be calling this function from within a
  // task, where the context will have changed.
  return legion_runtime_->get_executing_processor(Legion::Runtime::get_context());
}

inline bool Runtime::executing_inline_task() const noexcept { return executing_inline_task_; }

inline void Runtime::inline_task_start() noexcept { executing_inline_task_ = true; }

inline void Runtime::inline_task_end() noexcept
{
  LEGATE_ASSERT(executing_inline_task_);
  executing_inline_task_ = false;
}

}  // namespace legate::detail
