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

#include "core/comm/communicator.h"
#include "core/data/detail/physical_array.h"
#include "core/data/scalar.h"
#include "core/mapping/detail/machine.h"
#include "core/task/detail/return_value.h"
#include "core/task/detail/returned_exception.h"
#include "core/task/detail/task_return.h"
#include "core/utilities/internal_shared_ptr.h"

#include <optional>
#include <string_view>
#include <vector>

struct CUstream_st;

namespace legate::detail {

class TaskContext {
 public:
  TaskContext(const Legion::Task* task,
              LegateVariantCode variant_kind,
              const std::vector<Legion::PhysicalRegion>& regions);

  [[nodiscard]] std::vector<InternalSharedPtr<PhysicalArray>>& inputs();
  [[nodiscard]] std::vector<InternalSharedPtr<PhysicalArray>>& outputs();
  [[nodiscard]] std::vector<InternalSharedPtr<PhysicalArray>>& reductions();
  [[nodiscard]] const std::vector<legate::Scalar>& scalars() const noexcept;
  [[nodiscard]] const std::vector<comm::Communicator>& communicators() const noexcept;

  [[nodiscard]] std::int64_t task_id() const noexcept;
  [[nodiscard]] LegateVariantCode variant_kind() const noexcept;
  [[nodiscard]] bool is_single_task() const;
  [[nodiscard]] bool can_raise_exception() const;
  [[nodiscard]] const DomainPoint& get_task_index() const;
  [[nodiscard]] const Domain& get_launch_domain() const;

  void set_exception(ReturnedException what);
  [[nodiscard]] std::optional<ReturnedException>& get_exception() noexcept;

  [[nodiscard]] const mapping::detail::Machine& machine() const;
  [[nodiscard]] std::string_view get_provenance() const;

  /**
   * @brief Makes all of unbound output stores of this task empty
   */
  void make_all_unbound_stores_empty();
  [[nodiscard]] TaskReturn pack_return_values() const;
  [[nodiscard]] TaskReturn pack_return_values_with_exception(const ReturnedException& exn) const;

  [[nodiscard]] CUstream_st* get_task_stream() const;

 private:
  [[nodiscard]] std::vector<ReturnValue> get_return_values_() const;

  const Legion::Task* task_{};
  LegateVariantCode variant_kind_{};
  const std::vector<Legion::PhysicalRegion>& regions_;

  std::vector<InternalSharedPtr<PhysicalArray>> inputs_{}, outputs_{}, reductions_{};
  std::vector<InternalSharedPtr<PhysicalStore>> unbound_stores_{};
  std::vector<InternalSharedPtr<PhysicalStore>> scalar_stores_{};
  std::vector<legate::Scalar> scalars_{};
  std::vector<comm::Communicator> comms_{};
  bool can_raise_exception_{};
  mapping::detail::Machine machine_{};
  std::optional<ReturnedException> excn_{std::nullopt};
};

}  // namespace legate::detail

#include "core/task/detail/task_context.inl"
