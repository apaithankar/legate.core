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

#include "core/experimental/stl/detail/registrar.hpp"
#include "legate.h"

#include <gtest/gtest.h>

class DefaultFixture : public ::testing::Test {
 public:
  static void init(int argc, char** argv)
  {
    DefaultFixture::argc_ = argc;
    DefaultFixture::argv_ = argv;
  }

  void SetUp() override { EXPECT_EQ(legate::start(argc_, argv_), 0); }
  void TearDown() override { EXPECT_EQ(legate::finish(), 0); }

 private:
  inline static int argc_;
  inline static char** argv_;
};

class DeathTestFixture : public ::testing::Test {
 public:
  static void init(int argc, char** argv)
  {
    argc_ = argc;
    argv_ = argv;
  }

  inline static int argc_;
  inline static char** argv_;
};

class LegateSTLFixture : public ::testing::Test {
 public:
  static void init(int argc, char** argv)
  {
    LegateSTLFixture::argc_ = argc;
    LegateSTLFixture::argv_ = argv;
  }

  void SetUp() override { init_.emplace(argc_, argv_); }
  void TearDown() override { init_.reset(); }

 private:
  inline static std::optional<legate::stl::initialize_library> init_;
  inline static int argc_;
  inline static char** argv_;
};
