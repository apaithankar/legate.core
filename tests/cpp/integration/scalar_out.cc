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

namespace scalarout {

// NOLINTBEGIN(readability-magic-numbers)

struct Copy : public legate::LegateTask<Copy> {
  static constexpr std::int32_t TASK_ID = 0;

  static void cpu_variant(legate::TaskContext context)
  {
    auto input  = context.input(0).data();
    auto output = context.output(0).data();
    auto shape  = output.shape<1>();
    if (shape.empty()) {
      return;
    }
    auto out_acc = output.write_accessor<int64_t, 1>();
    auto in_acc  = input.read_accessor<int64_t, 1>();
    out_acc[0]   = in_acc[0];
  }
};

class Config {
 public:
  static constexpr std::string_view LIBRARY_NAME = "test_scalar_out";
  static void registration_callback(legate::Library library) { Copy::register_variants(library); }
};

class ScalarOut : public RegisterOnceFixture<Config> {};

void test_scalar_out()
{
  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->find_library(Config::LIBRARY_NAME);

  const legate::Shape extents{16};
  auto input  = runtime->create_store(extents, legate::int64(), false /* optimize_scalar */);
  auto output = runtime->create_store(legate::Scalar{int64_t{0}});

  runtime->issue_fill(input, legate::Scalar{int64_t{123}});

  {
    auto sliced   = input.slice(0, legate::Slice{2, 3});
    auto task     = runtime->create_task(library, Copy::TASK_ID);
    auto part_in  = task.add_input(sliced);
    auto part_out = task.add_output(output);
    task.add_constraint(legate::align(part_in, part_out));
    runtime->submit(std::move(task));
  }

  auto p_out = output.get_physical_store();
  auto acc   = p_out.read_accessor<int64_t, 1>();
  EXPECT_EQ(acc[0], 123);
}

TEST_F(ScalarOut, All) { test_scalar_out(); }

// NOLINTEND(readability-magic-numbers)

}  // namespace scalarout
