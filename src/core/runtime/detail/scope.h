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

#include "core/legate_c.h"
#include "core/mapping/detail/machine.h"
#include "core/runtime/exception_mode.h"
#include "core/utilities/internal_shared_ptr.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace legate::detail {

class Scope {
  using Machine = legate::mapping::detail::Machine;

 public:
  [[nodiscard]] std::int32_t priority() const;
  [[nodiscard]] ExceptionMode exception_mode() const;
  [[nodiscard]] std::string_view provenance() const;
  [[nodiscard]] const InternalSharedPtr<Machine>& machine() const;

  [[nodiscard]] std::int32_t exchange_priority(std::int32_t priority);
  [[nodiscard]] ExceptionMode exchange_exception_mode(ExceptionMode exception_mode);
  [[nodiscard]] std::string exchange_provenance(std::string provenance);
  [[nodiscard]] InternalSharedPtr<Machine> exchange_machine(InternalSharedPtr<Machine> machine);

 private:
  std::int32_t priority_{LEGATE_CORE_DEFAULT_TASK_PRIORITY};
  ExceptionMode exception_mode_{ExceptionMode::IMMEDIATE};
  std::string provenance_{};
  InternalSharedPtr<Machine> machine_{};
};

}  // namespace legate::detail

#include "core/runtime/detail/scope.inl"
