/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <legate/utilities/scope_guard.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <utilities/utilities.h>

namespace legate_scope_fail_test {

using ScopeFailUnit = ::testing::Test;

TEST_F(ScopeFailUnit, Construct)
{
  struct Callable {
    void operator()() noexcept {}
  };

  const legate::ScopeFail<Callable> guard{Callable{}};
  // nothing to do...
}

TEST_F(ScopeFailUnit, ConstructFromHelper)
{
  // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
  const auto guard = legate::make_scope_fail([]() noexcept {});
  // nothing to do
}

TEST_F(ScopeFailUnit, ConstructAndExecute)
{
  auto executed = false;
  {
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    const auto guard = legate::make_scope_fail([&]() noexcept { executed = true; });

    // ensure that func was never run
    EXPECT_FALSE(executed);
  }
  EXPECT_FALSE(executed);
}

TEST_F(ScopeFailUnit, ConstructAndExecuteThrow)
{
  auto executed = false;
  try {
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    const auto guard = legate::make_scope_fail([&]() noexcept { executed = true; });

    // ensure that func was never run
    EXPECT_FALSE(executed);
    throw std::runtime_error{"foo"};
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "foo");
    EXPECT_TRUE(executed);
  }
  EXPECT_TRUE(executed);
}

TEST_F(ScopeFailUnit, ConstructAndExecuteUnrelatedThrow)
{
  auto executed         = false;
  auto caught_outer_exn = false;
  try {
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores)
    const auto guard = legate::make_scope_fail([&]() noexcept { executed = true; });
    auto thrower     = [] { throw std::runtime_error{"from inside lambda"}; };
    auto caught_exn  = false;

    try {
      thrower();
    } catch (const std::exception& e) {
      caught_exn = true;
      ASSERT_STREQ(e.what(), "from inside lambda");
      EXPECT_FALSE(executed);
    }
    // To ensure the compiler doesn't optimize this stuff away...
    EXPECT_TRUE(caught_exn);
    // ensure that func was never run
    EXPECT_FALSE(executed);
    throw std::runtime_error{"foo"};
  } catch (const std::exception& e) {
    caught_outer_exn = true;
    ASSERT_STREQ(e.what(), "foo");
  }
  EXPECT_TRUE(caught_outer_exn);
  EXPECT_TRUE(executed);
}

TEST_F(ScopeFailUnit, FromMacro)
{
  auto executed1  = false;
  auto executed2  = false;
  auto executed3  = false;
  auto executed4  = false;
  auto caught_exn = false;
  try {
    // clang-format off
    LEGATE_SCOPE_FAIL(
      for (auto& exec : {&executed1, &executed2, &executed3, &executed4}) {
        *exec = true;
      }
    );
    // clang-format on

    throw std::runtime_error{"failure"};
  } catch (const std::exception& e) {
    caught_exn = true;
    ASSERT_STREQ(e.what(), "failure");
  }
  EXPECT_TRUE(caught_exn);
  EXPECT_TRUE(executed1);
  EXPECT_TRUE(executed2);
  EXPECT_TRUE(executed3);
  EXPECT_TRUE(executed4);
}

}  // namespace legate_scope_fail_test
