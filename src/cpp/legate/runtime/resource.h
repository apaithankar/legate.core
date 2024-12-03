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

#include <legate/utilities/detail/doxygen.h>

#include <cstdint>

namespace legate {

/**
 * @addtogroup runtime
 * @{
 */

/**
 * @brief POD for library configuration.
 */
struct ResourceConfig {
  /**
   * @brief Maximum number of tasks that the library can register
   */
  std::int64_t max_tasks{1024};  // NOLINT(readability-magic-numbers)
  /**
   * @brief Maximum number of dynamic tasks that the library can register (cannot exceed max_tasks)
   */
  std::int64_t max_dyn_tasks{0};
  /**
   * @brief Maximum number of custom reduction operators that the library can register
   */
  std::int64_t max_reduction_ops{};
  std::int64_t max_projections{};
  std::int64_t max_shardings{};
};

/** @} */

}  // namespace legate
