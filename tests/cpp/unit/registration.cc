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

#include <legate.h>

#include <gtest/gtest.h>

#include <utilities/utilities.h>

namespace test_registration {

using Registration = DefaultFixture;

namespace {

template <std::int32_t ID>
struct CPUVariantTask : public legate::LegateTask<CPUVariantTask<ID>> {
  static constexpr auto TASK_ID = legate::LocalTaskID{ID};
  static void cpu_variant(legate::TaskContext /*context*/) {}
};

template <std::int32_t ID>
struct GPUVariantTask : public legate::LegateTask<GPUVariantTask<ID>> {
  static constexpr auto TASK_ID = legate::LocalTaskID{ID};
  static void gpu_variant(legate::TaskContext /*context*/) {}
};

void test_duplicates()
{
  auto* runtime = legate::Runtime::get_runtime();
  auto library  = runtime->create_library("test_registration.libA");
  test_registration::CPUVariantTask<0>::register_variants(library);
  EXPECT_THROW(test_registration::CPUVariantTask<0>::register_variants(library),
               std::invalid_argument);
}

void test_out_of_bounds_task_id()
{
  legate::ResourceConfig config;
  config.max_tasks = 1;
  auto* runtime    = legate::Runtime::get_runtime();
  auto library     = runtime->create_library("test_registration.libB", config);

  EXPECT_THROW(test_registration::CPUVariantTask<1>::register_variants(library), std::out_of_range);
}

}  // namespace

TEST_F(Registration, Duplicate) { test_duplicates(); }

TEST_F(Registration, TaskIDOutOfBounds) { test_out_of_bounds_task_id(); }

}  // namespace test_registration
