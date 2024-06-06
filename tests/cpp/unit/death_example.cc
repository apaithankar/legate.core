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
#include "core/utilities/detail/strtoll.h"

#include "legate.h"
#include "utilities/utilities.h"

#include <gtest/gtest.h>
#include <unistd.h>

namespace unit {

using ExampleDeathTest = DeathTestNoInitFixture;

void kill_process(int /*argc*/, char** /*argv*/)
{
  (void)legate::start(0, nullptr);
  std::abort();
}

TEST_F(ExampleDeathTest, Simple)
{
  const auto value           = std::getenv("REALM_BACKTRACE");
  const bool realm_backtrace = value != nullptr && legate::detail::safe_strtoll(value) != 0;

  if (realm_backtrace) {
    // We can't check that the subprocess dies with SIGABRT when we run with REALM_BACKTRACE=1,
    // because Realm's signal handler doesn't propagate the signal, instead it does exit(1).
    // Even worse, when ASAN is used this triggers a segfault in Realm's signal handler, which
    // causes it to abort() instead of exit(1), so for now we just don't check the exit code
    // at all.

    EXPECT_DEATH(kill_process(argc_, argv_), "");
  } else {
    EXPECT_EXIT(kill_process(argc_, argv_), ::testing::KilledBySignal(SIGABRT), "");
  }
}

}  // namespace unit
