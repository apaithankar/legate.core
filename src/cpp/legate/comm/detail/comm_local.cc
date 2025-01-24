/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved. SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include <legate/comm/detail/comm_local.h>

#include <legate_defines.h>

#include <legate/comm/coll.h>
#include <legate/comm/detail/comm_cpu_factory.h>
#include <legate/task/detail/legion_task.h>
#include <legate/utilities/detail/core_ids.h>
#include <legate/utilities/macros.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace legate::detail {

extern void show_progress(const Legion::Task* task, Legion::Context ctx, Legion::Runtime* runtime);

}  // namespace legate::detail

namespace legate::detail::comm::local {

class InitMapping : public detail::LegionTask<InitMapping> {
 public:
  static constexpr auto TASK_ID = LocalTaskID{CoreTask::INIT_CPUCOLL_MAPPING};

  static int cpu_variant(const Legion::Task* task,
                         const std::vector<Legion::PhysicalRegion>& /*regions*/,
                         Legion::Context context,
                         Legion::Runtime* runtime)
  {
    legate::detail::show_progress(task, context, runtime);

    return 0;
  }

#if LEGATE_DEFINED(LEGATE_USE_OPENMP)
  static int omp_variant(const Legion::Task* task,
                         const std::vector<Legion::PhysicalRegion>& regions,
                         Legion::Context context,
                         Legion::Runtime* runtime)
  {
    return cpu_variant(task, regions, context, runtime);
  }
#endif

#if LEGATE_DEFINED(LEGATE_USE_CUDA)
  static int gpu_variant(const Legion::Task* task,
                         const std::vector<Legion::PhysicalRegion>& regions,
                         Legion::Context context,
                         Legion::Runtime* runtime)
  {
    return cpu_variant(task, regions, context, runtime);
  }
#endif
};

class Init : public detail::LegionTask<Init> {
 public:
  static constexpr auto TASK_ID = LocalTaskID{CoreTask::INIT_CPUCOLL};

  static constexpr auto CPU_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);
  static constexpr auto GPU_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);
  static constexpr auto OMP_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);

  static legate::comm::coll::CollComm cpu_variant(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& /*regions*/,
    Legion::Context context,
    Legion::Runtime* runtime)
  {
    legate::detail::show_progress(task, context, runtime);

    const auto point     = static_cast<std::size_t>(task->index_point[0]);
    const auto num_ranks = task->index_domain.get_volume();

    LEGATE_CHECK(task->futures.size() == num_ranks + 1);
    const auto unique_id = task->futures[0].get_result<int>();
    auto comm            = std::make_unique<legate::comm::coll::Coll_Comm>();

    legate::comm::coll::collCommCreate(
      comm.get(), static_cast<int>(num_ranks), static_cast<int>(point), unique_id, nullptr);
    return comm.release();
  }

#if LEGATE_DEFINED(LEGATE_USE_OPENMP)
  static legate::comm::coll::CollComm omp_variant(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context context,
    Legion::Runtime* runtime)
  {
    return cpu_variant(task, regions, context, runtime);
  }
#endif

#if LEGATE_DEFINED(LEGATE_USE_CUDA)
  static legate::comm::coll::CollComm gpu_variant(
    const Legion::Task* task,
    const std::vector<Legion::PhysicalRegion>& regions,
    Legion::Context context,
    Legion::Runtime* runtime)
  {
    return cpu_variant(task, regions, context, runtime);
  }
#endif
};

class Finalize : public detail::LegionTask<Finalize> {
 public:
  static constexpr auto TASK_ID = LocalTaskID{CoreTask::FINALIZE_CPUCOLL};

  static constexpr auto CPU_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);
  static constexpr auto GPU_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);
  static constexpr auto OMP_VARIANT_OPTIONS = legate::VariantOptions{}.with_concurrent(true);

  static void cpu_variant(const Legion::Task* task,
                          const std::vector<Legion::PhysicalRegion>& /*regions*/,
                          Legion::Context context,
                          Legion::Runtime* runtime)
  {
    legate::detail::show_progress(task, context, runtime);

    LEGATE_CHECK(task->futures.size() == 1);
    std::unique_ptr<legate::comm::coll::Coll_Comm> comm{
      task->futures[0].get_result<legate::comm::coll::CollComm>()};

    LEGATE_CHECK(comm->global_rank == static_cast<int>(task->index_point[0]));
    legate::comm::coll::collCommDestroy(comm.get());
  }

#if LEGATE_DEFINED(LEGATE_USE_OPENMP)
  static void omp_variant(const Legion::Task* task,
                          const std::vector<Legion::PhysicalRegion>& regions,
                          Legion::Context context,
                          Legion::Runtime* runtime)
  {
    cpu_variant(task, regions, context, runtime);
  }
#endif

#if LEGATE_DEFINED(LEGATE_USE_CUDA)
  static void gpu_variant(const Legion::Task* task,
                          const std::vector<Legion::PhysicalRegion>& regions,
                          Legion::Context context,
                          Legion::Runtime* runtime)
  {
    cpu_variant(task, regions, context, runtime);
  }
#endif
};

void register_tasks(const legate::Library& core_library)
{
  InitMapping::register_variants(core_library);
  Init::register_variants(core_library);
  Finalize::register_variants(core_library);
}

std::unique_ptr<CommunicatorFactory> make_factory(const detail::Library* library)
{
  return std::make_unique<cpu::Factory<Init, InitMapping, Finalize>>(library);
}

}  // namespace legate::detail::comm::local
