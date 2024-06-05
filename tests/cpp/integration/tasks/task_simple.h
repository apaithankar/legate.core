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

#include "core/mapping/mapping.h"

#include "legate.h"

namespace task::simple {

enum TaskOpCode {
  _OP_CODE_BASE = 0,
  HELLO         = 1,
  WRITER        = 2,
  REDUCER       = 3,
};

inline constexpr std::string_view LIBRARY_NAME = "legate.simple";

extern Legion::Logger logger;

void register_tasks();

struct HelloTask : public legate::LegateTask<HelloTask> {
  static constexpr std::int32_t TASK_ID = HELLO;
  static void cpu_variant(legate::TaskContext context);
};

struct WriterTask : public legate::LegateTask<WriterTask> {
  static constexpr std::int32_t TASK_ID = WRITER;
  static void cpu_variant(legate::TaskContext context);
};

struct ReducerTask : public legate::LegateTask<ReducerTask> {
  static constexpr std::int32_t TASK_ID = REDUCER;
  static void cpu_variant(legate::TaskContext context);
};

}  // namespace task::simple
