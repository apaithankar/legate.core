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

#include "core/comm/comm_cpu.h"

#include "core/comm/backend_network.h"
#include "core/comm/coll.h"
#include "core/comm/comm_util.h"
#include "core/operation/detail/task_launcher.h"
#include "core/runtime/detail/communicator_manager.h"
#include "core/runtime/detail/library.h"
#include "core/runtime/detail/runtime.h"
#include "core/runtime/runtime.h"
#include "core/utilities/detail/zip.h"

#include <memory>
#include <vector>

namespace legate::detail {

void show_progress(const Legion::Task* task, Legion::Context ctx, Legion::Runtime* runtime);

}  // namespace legate::detail

namespace legate::comm::cpu {

class Factory final : public detail::CommunicatorFactory {
 public:
  explicit Factory(const detail::Library* core_library);

  [[nodiscard]] bool needs_barrier() const override { return false; }
  [[nodiscard]] bool is_supported_target(mapping::TaskTarget target) const override;

 protected:
  [[nodiscard]] Legion::FutureMap initialize_(const mapping::detail::Machine& machine,
                                              std::uint32_t num_tasks) override;
  void finalize_(const mapping::detail::Machine& machine,
                 std::uint32_t num_tasks,
                 const Legion::FutureMap& communicator) override;

 private:
  const detail::Library* core_library_{};
};

Factory::Factory(const detail::Library* core_library) : core_library_{core_library} {}

bool Factory::is_supported_target(mapping::TaskTarget /*target*/) const { return true; }

Legion::FutureMap Factory::initialize_(const mapping::detail::Machine& machine,
                                       std::uint32_t num_tasks)
{
  const Domain launch_domain{
    Rect<1>{Point<1>{0}, Point<1>{static_cast<std::int64_t>(num_tasks) - 1}}};
  auto tag =
    machine.preferred_target == mapping::TaskTarget::OMP ? LEGATE_OMP_VARIANT : LEGATE_CPU_VARIANT;

  // Generate a unique ID
  auto comm_id = Legion::Future::from_value<std::int32_t>(coll::collInitComm());

  // Find a mapping of all participants
  detail::TaskLauncher init_cpucoll_mapping_launcher{
    core_library_, machine, LEGATE_CORE_INIT_CPUCOLL_MAPPING_TASK_ID, tag};
  init_cpucoll_mapping_launcher.add_future(comm_id);
  auto mapping = init_cpucoll_mapping_launcher.execute(launch_domain);

  // Then create communicators on participating processors
  detail::TaskLauncher init_cpucoll_launcher{
    core_library_, machine, LEGATE_CORE_INIT_CPUCOLL_TASK_ID, tag};
  init_cpucoll_launcher.add_future(comm_id);
  init_cpucoll_launcher.set_concurrent(true);

  auto domain = mapping.get_future_map_domain();
  for (Domain::DomainPointIterator it{domain}; it; ++it) {
    init_cpucoll_launcher.add_future(mapping.get_future(*it));
  }
  return init_cpucoll_launcher.execute(launch_domain);
}

void Factory::finalize_(const mapping::detail::Machine& machine,
                        std::uint32_t num_tasks,
                        const Legion::FutureMap& communicator)
{
  const auto tag =
    machine.preferred_target == mapping::TaskTarget::OMP ? LEGATE_OMP_VARIANT : LEGATE_CPU_VARIANT;
  const Domain launch_domain{
    Rect<1>{Point<1>{0}, Point<1>{static_cast<std::int64_t>(num_tasks) - 1}}};
  detail::TaskLauncher launcher{core_library_, machine, LEGATE_CORE_FINALIZE_CPUCOLL_TASK_ID, tag};

  launcher.set_concurrent(true);
  launcher.add_future_map(communicator);
  launcher.execute(launch_domain);
}

namespace {

int init_cpucoll_mapping(const Legion::Task* task,
                         const std::vector<Legion::PhysicalRegion>& /*regions*/,
                         Legion::Context context,
                         Legion::Runtime* runtime)
{
  legate::detail::show_progress(task, context, runtime);
  // clang-tidy cannot see the MPI_Comm_rank() call below
  int mpi_rank = 0;  // NOLINT(misc-const-correctness)
#if LEGATE_DEFINED(LEGATE_USE_NETWORK)
  if (coll::backend_network->comm_type == coll::CollCommType::CollMPI) {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  }
#endif

  return mpi_rank;
}

coll::CollComm init_cpucoll(const Legion::Task* task,
                            const std::vector<Legion::PhysicalRegion>& /*regions*/,
                            Legion::Context context,
                            Legion::Runtime* runtime)
{
  legate::detail::show_progress(task, context, runtime);

  const auto point     = static_cast<std::size_t>(task->index_point[0]);
  const auto num_ranks = task->index_domain.get_volume();

  LEGATE_CHECK(task->futures.size() == num_ranks + 1);
  const auto unique_id = task->futures[0].get_result<int>();
  auto comm            = std::make_unique<coll::Coll_Comm>();
  auto mapping_table   = std::vector<int>{};

  if (LEGATE_DEFINED(LEGATE_USE_NETWORK) &&
      (coll::backend_network->comm_type == coll::CollCommType::CollMPI)) {
    mapping_table.reserve(num_ranks);
    for (std::size_t i = 0; i < num_ranks; ++i) {
      const auto mapping_table_element = task->futures[i + 1].get_result<int>();

      mapping_table.push_back(mapping_table_element);
    }
    LEGATE_CHECK(mapping_table[point] == comm->mpi_rank);
  }

  coll::collCommCreate(comm.get(),
                       static_cast<int>(num_ranks),
                       static_cast<int>(point),
                       unique_id,
                       mapping_table.data());
  return comm.release();
}

void finalize_cpucoll(const Legion::Task* task,
                      const std::vector<Legion::PhysicalRegion>& /*regions*/,
                      Legion::Context context,
                      Legion::Runtime* runtime)
{
  legate::detail::show_progress(task, context, runtime);

  LEGATE_CHECK(task->futures.size() == 1);
  std::unique_ptr<coll::Coll_Comm> comm{task->futures[0].get_result<coll::CollComm>()};

  LEGATE_CHECK(comm->global_rank == static_cast<int>(task->index_point[0]));
  coll::collCommDestroy(comm.get());
}

}  // namespace

void register_tasks(const detail::Library* core_library)
{
  const auto runtime = Legion::Runtime::get_runtime();

  // TODO(wonchanl): The following should use the Legate API to register task variants, instead of
  // the Legion API. We can't quite do that today because the tasks have return values, which Legate
  // currently wouldn't understand.

  // Register the task variants
  const auto proc_kinds = {Processor::LOC_PROC, Processor::TOC_PROC, Processor::OMP_PROC};
  const auto variants   = {LEGATE_CPU_VARIANT, LEGATE_GPU_VARIANT, LEGATE_OMP_VARIANT};
  for (auto&& [proc_kind, variant] : legate::detail::zip_equal(proc_kinds, variants)) {
    runtime->register_task_variant<int, init_cpucoll_mapping>(
      detail::make_registrar(core_library,
                             LEGATE_CORE_INIT_CPUCOLL_MAPPING_TASK_ID,
                             "core::comm::cpu::init_mapping",
                             proc_kind,
                             false),
      variant);

    runtime->register_task_variant<coll::CollComm, init_cpucoll>(
      detail::make_registrar(
        core_library, LEGATE_CORE_INIT_CPUCOLL_TASK_ID, "core::comm::cpu::init", proc_kind, true),
      variant);

    runtime->register_task_variant<finalize_cpucoll>(
      detail::make_registrar(core_library,
                             LEGATE_CORE_FINALIZE_CPUCOLL_TASK_ID,
                             "core::comm::cpu::finalize",
                             proc_kind,
                             true),
      variant);
  }
}

void register_factory(const detail::Library* library)
{
  auto* comm_mgr = detail::Runtime::get_runtime()->communicator_manager();
  comm_mgr->register_factory("cpu", std::make_unique<Factory>(library));
}

}  // namespace legate::comm::cpu
