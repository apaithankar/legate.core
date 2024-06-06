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

namespace alignment_constraints {

// NOLINTBEGIN(readability-magic-numbers)

using Alignment = DefaultFixture;

namespace {

constexpr std::string_view LIBRARY_NAME = "test_alignment_constraints";

}  // namespace

enum TaskIDs : std::uint8_t {
  INIT                       = 0,
  ALIGNMENT_TESTER           = 0,
  ALIGNMENT_BROADCAST_TESTER = 3,
  TRANSFORMED_TESTER         = 6,
};

// Dummy task to make the runtime think the store is initialized
struct Initializer : public legate::LegateTask<Initializer> {
  static constexpr std::int32_t TASK_ID = INIT;

  static void cpu_variant(legate::TaskContext /*context*/) {}
};

template <std::int32_t DIM>
struct AlignmentTester : public legate::LegateTask<AlignmentTester<DIM>> {
  static constexpr std::int32_t TASK_ID = ALIGNMENT_TESTER + DIM;

  static void cpu_variant(legate::TaskContext context)
  {
    auto outputs = context.outputs();
    auto shape   = outputs.at(0).shape<DIM>();
    for (auto& output : outputs) {
      EXPECT_EQ(shape, output.shape<DIM>());
    }
  }
};

template <std::int32_t DIM>
struct AlignmentBroadcastTester : public legate::LegateTask<AlignmentBroadcastTester<DIM>> {
  static constexpr std::int32_t TASK_ID = ALIGNMENT_BROADCAST_TESTER + DIM;

  static void cpu_variant(legate::TaskContext context)
  {
    auto shape1 = context.outputs().at(0).shape<DIM>();
    auto shape2 = context.outputs().at(1).shape<DIM>();
    auto extent = context.scalars().at(0).value<std::size_t>();
    EXPECT_EQ(shape1, shape2);
    EXPECT_EQ(shape1.lo[0], 0);
    EXPECT_EQ(shape1.hi[0] + 1, extent);
    EXPECT_EQ(shape1.hi[0] - shape1.lo[0] + 1, extent);
  }
};

template <std::int32_t DIM>
struct TransformedTester : public legate::LegateTask<TransformedTester<DIM>> {
  static constexpr std::int32_t TASK_ID = TRANSFORMED_TESTER + DIM;

  static void cpu_variant(legate::TaskContext context)
  {
    auto shape1 = context.inputs().at(0).shape<DIM>();
    auto shape2 = context.inputs().at(1).shape<DIM>();
    EXPECT_EQ(shape1, shape2);
  }
};

void prepare()
{
  static bool prepared = false;
  if (prepared) {
    return;
  }
  prepared     = true;
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->create_library(LIBRARY_NAME);
  Initializer::register_variants(context);
  AlignmentTester<1>::register_variants(context);
  AlignmentTester<2>::register_variants(context);
  AlignmentTester<3>::register_variants(context);
  AlignmentBroadcastTester<1>::register_variants(context);
  AlignmentBroadcastTester<2>::register_variants(context);
  AlignmentBroadcastTester<3>::register_variants(context);
  TransformedTester<1>::register_variants(context);
  TransformedTester<2>::register_variants(context);
  TransformedTester<3>::register_variants(context);
}

void test_alignment()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);

  auto launch_tester = [&](const std::vector<std::uint64_t>& extents) {
    auto store1 = runtime->create_store(legate::Shape{extents}, legate::int64());
    auto store2 = runtime->create_store(legate::Shape{extents}, legate::int64());
    auto store3 = runtime->create_store(legate::Shape{extents}, legate::int64());

    auto task =
      runtime->create_task(context, ALIGNMENT_TESTER + static_cast<std::int64_t>(extents.size()));
    auto part1 = task.add_output(store1);
    auto part2 = task.add_output(store2);
    auto part3 = task.add_output(store3);

    task.add_constraint(legate::align(part1, part2));
    task.add_constraint(legate::align(part2, part3));

    runtime->submit(std::move(task));
  };

  launch_tester({10});
  launch_tester({10, 10});
  launch_tester({10, 10, 10});
}

void test_alignment_and_broadcast()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);

  auto launch_tester = [&](const std::vector<std::uint64_t>& extents) {
    auto store1 = runtime->create_store(legate::Shape{extents}, legate::int64());
    auto store2 = runtime->create_store(legate::Shape{extents}, legate::int64());

    auto task = runtime->create_task(
      context, ALIGNMENT_BROADCAST_TESTER + static_cast<std::int64_t>(extents.size()));
    auto part1 = task.add_output(store1);
    auto part2 = task.add_output(store2);

    task.add_scalar_arg(legate::Scalar(extents[0]));

    task.add_constraint(legate::align(part1, part2));
    task.add_constraint(legate::broadcast(part1, legate::tuple<std::uint32_t>{0}));

    runtime->submit(std::move(task));
  };

  launch_tester({100});
  launch_tester({100, 10});
  launch_tester({100, 10, 20});
}

void initialize(const legate::LogicalStore& store)
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);
  auto task    = runtime->create_task(context, INIT);

  task.add_output(store);

  runtime->submit(std::move(task));
}

void test_alignment_transformed()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);

  auto launch_tester = [&](auto store1, auto store2) {
    auto task =
      runtime->create_task(context, static_cast<std::int64_t>(TRANSFORMED_TESTER) + store1.dim());
    auto part1 = task.add_input(store1);
    auto part2 = task.add_input(store2);

    task.add_constraint(legate::align(part1, part2));

    runtime->submit(std::move(task));
  };

  // TODO(wonchanl): Extents are chosen such that imaginary dimensions are not partitioned.
  // Such scenarios should be tested once we pass down partition metadata to the tasks and have
  // them compute extents on imaginary dimensions based on that
  auto store1 = runtime->create_store(legate::Shape{100}, legate::int64());
  auto store2 = runtime->create_store(legate::Shape{100, 10}, legate::int64());
  auto store3 = runtime->create_store(legate::Shape{100, 10, 2}, legate::int64());
  auto store4 = runtime->create_store(legate::Shape{10, 10}, legate::int64());

  initialize(store1);
  initialize(store2);
  initialize(store3);
  initialize(store4);

  launch_tester(store1.promote(1, 10), store2);
  launch_tester(store1, store2.project(1, 5));
  launch_tester(store1.promote(1, 10), store3.project(2, 0));
  launch_tester(store1.promote(1, 10).promote(2, 2), store3);
  launch_tester(store1.promote(1, 5), store2.slice(1, legate::Slice(3, 8)));
  launch_tester(store1.promote(0, 10).transpose({1, 0}), store2);
  launch_tester(store1.delinearize(0, {10, 10}), store4);
  launch_tester(store4.transpose({1, 0}).promote(0, 1), store4.promote(0, 1));
}

void test_redundant_alignment()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);

  const std::vector<std::uint64_t> extents = {10};
  auto shapes                              = legate::Shape{extents};
  auto store1                              = runtime->create_store(shapes, legate::int64());
  auto store2                              = runtime->create_store(shapes, legate::int64());
  auto store3                              = runtime->create_store(shapes, legate::int64());

  auto task =
    runtime->create_task(context, ALIGNMENT_TESTER + static_cast<std::int64_t>(extents.size()));
  auto part1 = task.add_output(store1);
  auto part2 = task.add_output(store2);
  auto part3 = task.add_output(store3);

  task.add_constraint(legate::align(part1, part2));
  task.add_constraint(legate::align(part2, part3));

  // Redundant alignments
  task.add_constraint(legate::align(part1, part2));
  task.add_constraint(legate::align(part1, part3));
  task.add_constraint(legate::align(part2, part1));

  runtime->submit(std::move(task));
}

void test_invalid_alignment()
{
  auto runtime = legate::Runtime::get_runtime();
  auto context = runtime->find_library(LIBRARY_NAME);

  auto store1 = runtime->create_store(legate::Shape{10}, legate::int64());
  auto store2 = runtime->create_store(legate::Shape{9}, legate::int64());
  auto task =
    runtime->create_task(context, static_cast<std::int64_t>(TRANSFORMED_TESTER) + store1.dim());

  auto part1 = task.add_output(store1);
  auto part2 = task.add_output(store2);
  task.add_constraint(legate::align(part1, part2));
  EXPECT_THROW(runtime->submit(std::move(task)), std::invalid_argument);
}

TEST_F(Alignment, Basic)
{
  prepare();
  test_alignment();
}

TEST_F(Alignment, WithBroadcast)
{
  prepare();
  test_alignment_and_broadcast();
}

TEST_F(Alignment, WithTransform)
{
  prepare();
  test_alignment_transformed();
}

TEST_F(Alignment, Redundant)
{
  prepare();
  test_redundant_alignment();
}

TEST_F(Alignment, Invalid)
{
  prepare();
  test_invalid_alignment();
}

// NOLINTEND(readability-magic-numbers)

}  // namespace alignment_constraints
