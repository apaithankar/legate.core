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

#include "legate.h"
#include "utilities/utilities.h"

#include <gtest/gtest.h>

namespace req_analyzer {

// NOLINTBEGIN(readability-magic-numbers)

struct Tester : public legate::LegateTask<Tester> {
  static constexpr auto TASK_ID = legate::LocalTaskID{0};

  static void cpu_variant(legate::TaskContext context)
  {
    auto inputs  = context.inputs();
    auto outputs = context.outputs();
    for (auto& input : inputs) {
      (void)input.data().read_accessor<int64_t, 2>();
    }
    for (auto& output : outputs) {
      (void)output.data().read_accessor<int64_t, 2>();
      (void)output.data().write_accessor<int64_t, 2>();
    }
  }
};

class Config {
 public:
  static constexpr std::string_view LIBRARY_NAME = "test_req_analyzer";
  static void registration_callback(legate::Library library) { Tester::register_variants(library); }
};

class ReqAnalyzer : public RegisterOnceFixture<Config> {};

void test_inout_store()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(Config::LIBRARY_NAME);

  auto store1 = runtime->create_store(legate::Shape{10, 5}, legate::int64());
  auto store2 = runtime->create_store(legate::Shape{10, 5}, legate::int64());
  runtime->issue_fill(store1, legate::Scalar{std::int64_t{0}});
  runtime->issue_fill(store2, legate::Scalar{std::int64_t{0}});

  auto task  = runtime->create_task(context, Tester::TASK_ID);
  auto part1 = task.add_input(store1);
  auto part2 = task.add_input(store2);
  task.add_output(store1);
  task.add_constraint(legate::align(part1, part2));
  runtime->submit(std::move(task));
}

void test_isomorphic_transformed_stores()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(Config::LIBRARY_NAME);

  auto store = runtime->create_store(legate::Shape{10}, legate::int64());
  runtime->issue_fill(store, legate::Scalar{std::int64_t{0}});

  // Create aliased stores that are semantically equivalent
  auto promoted1 = store.promote(1, 5);
  auto promoted2 = store.promote(1, 5);
  auto task      = runtime->create_task(context, Tester::TASK_ID);
  task.add_input(promoted1);
  task.add_output(promoted2);
  runtime->submit(std::move(task));
}

TEST_F(ReqAnalyzer, InoutStore) { test_inout_store(); }

TEST_F(ReqAnalyzer, IsomorphicTransformedStores) { test_isomorphic_transformed_stores(); }

// NOLINTEND(readability-magic-numbers)

}  // namespace req_analyzer
