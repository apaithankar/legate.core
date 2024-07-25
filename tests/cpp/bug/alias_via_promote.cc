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

namespace alias_via_promote_test {

using AliasViaPromote = DefaultFixture;

// NOLINTBEGIN(readability-magic-numbers)

namespace {

constexpr std::string_view LIBRARY_NAME = "test_alias_via_promote";

}  // namespace

struct Checker : public legate::LegateTask<Checker> {
  static constexpr auto TASK_ID = legate::LocalTaskID{0};
  static void cpu_variant(legate::TaskContext /*context*/) {}
};

TEST_F(AliasViaPromote, Bug1)
{
  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->create_library(LIBRARY_NAME);
  Checker::register_variants(library);

  auto store = runtime->create_store(legate::Shape{2}, legate::int64());
  runtime->issue_fill(store, legate::Scalar{int64_t{42}});

  auto task  = runtime->create_task(library, Checker::TASK_ID);
  auto part1 = task.add_output(store.promote(1, 100));
  task.add_constraint(legate::broadcast(part1, {0}));
  runtime->submit(std::move(task));
}

// NOLINTEND(readability-magic-numbers)

}  // namespace alias_via_promote_test
