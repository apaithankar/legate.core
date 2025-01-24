/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
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

#include <legate/data/detail/logical_store.h>
#include <legate/operation/detail/operation.h>
#include <legate/partitioning/constraint.h>
#include <legate/utilities/internal_shared_ptr.h>

#include <optional>

namespace legate::detail {

class ConstraintSolver;

class Gather final : public Operation {
 public:
  Gather(InternalSharedPtr<LogicalStore> target,
         InternalSharedPtr<LogicalStore> source,
         InternalSharedPtr<LogicalStore> source_indirect,
         std::uint64_t unique_id,
         std::int32_t priority,
         mapping::detail::Machine machine,
         std::optional<std::int32_t> redop_kind);

  void set_indirect_out_of_range(bool flag);

  void validate() override;
  void launch(Strategy* strategy) override;

  void add_to_solver(ConstraintSolver& solver) override;

  [[nodiscard]] Kind kind() const override;
  [[nodiscard]] bool needs_flush() const override;

 private:
  bool out_of_range_{true};
  StoreArg target_{};
  StoreArg source_{};
  StoreArg source_indirect_{};
  InternalSharedPtr<Constraint> constraint_{};
  std::optional<std::int32_t> redop_kind_{};
};

}  // namespace legate::detail

#include <legate/operation/detail/gather.inl>
