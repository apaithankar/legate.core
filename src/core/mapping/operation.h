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

#include "core/data/scalar.h"
#include "core/mapping/array.h"

#include <vector>

/**
 * @file
 * @brief Class definitions for operations and stores used in mapping
 */

namespace legate::mapping {

namespace detail {
class Task;
}  // namespace detail

/**
 * @ingroup mapping
 * @brief A metadata class for tasks
 */
class Task {
 public:
  /**
   * @brief Returns the task id
   *
   * @return Task id
   */
  [[nodiscard]] int64_t task_id() const;

  /**
   * @brief Returns metadata for the task's input arrays
   *
   * @return Vector of array metadata objects
   */
  [[nodiscard]] std::vector<Array> inputs() const;
  /**
   * @brief Returns metadata for the task's output arrays
   *
   * @return Vector of array metadata objects
   */
  [[nodiscard]] std::vector<Array> outputs() const;
  /**
   * @brief Returns metadata for the task's reduction arrays
   *
   * @return Vector of array metadata objects
   */
  [[nodiscard]] std::vector<Array> reductions() const;
  /**
   * @brief Returns the vector of the task's by-value arguments. Unlike `mapping::Array`
   * objects that have no access to data in the arrays, the returned `Scalar` objects
   * contain valid arguments to the task
   *
   * @return Vector of `Scalar` objects
   */
  [[nodiscard]] const std::vector<Scalar>& scalars() const;

  /**
   * @brief Returns metadata for the task's input array
   *
   * @param index Index of the input array
   *
   * @return Array metadata object
   */
  [[nodiscard]] Array input(uint32_t index) const;
  /**
   * @brief Returns metadata for the task's output array
   *
   * @param index Index of the output array
   *
   * @return Array metadata object
   */
  [[nodiscard]] Array output(uint32_t index) const;
  /**
   * @brief Returns metadata for the task's reduction array
   *
   * @param index Index of the reduction array
   *
   * @return Array metadata object
   */
  [[nodiscard]] Array reduction(uint32_t index) const;

  /**
   * @brief Returns the number of task's inputs
   *
   * @return Number of arrays
   */
  [[nodiscard]] size_t num_inputs() const;
  /**
   * @brief Returns the number of task's outputs
   *
   * @return Number of arrays
   */
  [[nodiscard]] size_t num_outputs() const;
  /**
   * @brief Returns the number of task's reductions
   *
   * @return Number of arrays
   */
  [[nodiscard]] size_t num_reductions() const;

  explicit Task(detail::Task* impl);

  Task(const Task&)            = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&&)                 = delete;
  Task& operator=(Task&&)      = delete;

 private:
  [[nodiscard]] detail::Task* impl() const noexcept;

  detail::Task* impl_{};
};

}  // namespace legate::mapping

#include "core/mapping/operation.inl"
