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

#include "core/data/detail/logical_store.h"
#include "core/operation/detail/operation.h"
#include "core/partitioning/detail/constraint.h"
#include "core/utilities/internal_shared_ptr.h"

#include <optional>

namespace legate::detail {

class ConstraintSolver;

class Copy final : public Operation {
 public:
  Copy(InternalSharedPtr<LogicalStore> target,
       InternalSharedPtr<LogicalStore> source,
       std::uint64_t unique_id,
       std::int32_t priority,
       mapping::detail::Machine machine,
       std::optional<std::int32_t> redop_kind);

  void validate() override;
  void launch(Strategy* strategy) override;

  void add_to_solver(ConstraintSolver& solver) override;

  [[nodiscard]] Kind kind() const override;

 private:
  StoreArg target_{};
  StoreArg source_{};
  InternalSharedPtr<Alignment> constraint_{};
  std::optional<std::int32_t> redop_kind_{};
};

}  // namespace legate::detail

#include "core/operation/detail/copy.inl"
