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

#include "core/mapping/mapping.h"
#include "legate.h"
#include "scoping_cffi.h"

namespace scoping {

static const char* const library_name = "scoping";

Legion::Logger log_scoping(library_name);

template <typename T, int ID>
struct Task : public legate::LegateTask<T> {
  static constexpr int TASK_ID = ID;
};

namespace {

void validate(legate::TaskContext context)
{
  if (context.is_single_task()) return;

  int32_t num_tasks = context.get_launch_domain().get_volume();
  auto to_compare   = context.scalars().at(0).value<int32_t>();
  if (to_compare != num_tasks) {
    log_scoping.error("Test failed: expected %d tasks, but got %d tasks", to_compare, num_tasks);
    LEGATE_ABORT;
  }
}

}  // namespace

class MultiVariantTask : public Task<MultiVariantTask, MULTI_VARIANT> {
 public:
  static void cpu_variant(legate::TaskContext context) { validate(context); }
#ifdef LEGATE_USE_OPENMP
  static void omp_variant(legate::TaskContext context) { validate(context); }
#endif
#ifdef LEGATE_USE_CUDA
  static void gpu_variant(legate::TaskContext context) { validate(context); }
#endif
};

class CpuVariantOnlyTask : public Task<CpuVariantOnlyTask, CPU_VARIANT_ONLY> {
 public:
  static void cpu_variant(legate::TaskContext context) { validate(context); }
};

void registration_callback()
{
  auto context = legate::Runtime::get_runtime()->create_library(library_name);

  MultiVariantTask::register_variants(context);
  CpuVariantOnlyTask::register_variants(context);
}

}  // namespace scoping

extern "C" {

void perform_registration(void)
{
  legate::Core::perform_registration<scoping::registration_callback>();
}
}
