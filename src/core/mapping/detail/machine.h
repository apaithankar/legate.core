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

#pragma once

#include "core/mapping/machine.h"
#include "core/mapping/mapping.h"
#include "core/utilities/detail/buffer_builder.h"
#include "core/utilities/typedefs.h"

#include <cstdint>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace legate::mapping::detail {

class Machine {
 public:
  Machine() = default;
  explicit Machine(std::map<TaskTarget, ProcessorRange> processor_ranges);

  [[nodiscard]] const ProcessorRange& processor_range() const;
  [[nodiscard]] const ProcessorRange& processor_range(TaskTarget target) const;

  [[nodiscard]] std::vector<TaskTarget> valid_targets() const;
  [[nodiscard]] std::vector<TaskTarget> valid_targets_except(
    const std::set<TaskTarget>& to_exclude) const;

  [[nodiscard]] std::uint32_t count() const;
  [[nodiscard]] std::uint32_t count(TaskTarget target) const;

  [[nodiscard]] std::string to_string() const;

  void pack(legate::detail::BufferBuilder& buffer) const;

  [[nodiscard]] Machine only(TaskTarget target) const;
  [[nodiscard]] Machine only(const std::vector<TaskTarget>& targets) const;
  [[nodiscard]] Machine slice(std::uint32_t from,
                              std::uint32_t to,
                              TaskTarget target,
                              bool keep_others = false) const;
  [[nodiscard]] Machine slice(std::uint32_t from, std::uint32_t to, bool keep_others = false) const;

  [[nodiscard]] Machine operator[](TaskTarget target) const;
  [[nodiscard]] Machine operator[](const std::vector<TaskTarget>& targets) const;
  bool operator==(const Machine& other) const;
  bool operator!=(const Machine& other) const;
  [[nodiscard]] Machine operator&(const Machine& other) const;

  [[nodiscard]] bool empty() const;

  TaskTarget preferred_target{TaskTarget::CPU};
  std::map<TaskTarget, ProcessorRange> processor_ranges{};
};

std::ostream& operator<<(std::ostream& os, const Machine& machine);

class LocalProcessorRange {
 public:
  LocalProcessorRange() = default;
  LocalProcessorRange(std::uint32_t offset,
                      std::uint32_t total_proc_count,
                      const Processor* local_procs,
                      std::size_t num_local_procs);

  explicit LocalProcessorRange(const std::vector<Processor>& procs);

  [[nodiscard]] const Processor& first() const;
  [[nodiscard]] const Processor& operator[](std::uint32_t idx) const;

  [[nodiscard]] bool empty() const;
  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] std::uint32_t total_proc_count() const;

  friend std::ostream& operator<<(std::ostream& os, const LocalProcessorRange& range);

 private:
  std::uint32_t offset_{};
  std::uint32_t total_proc_count_{};
  Span<const Processor> procs_{};
};

// A machine object holding handles to local processors and memories
class LocalMachine {
 public:
  LocalMachine();

  [[nodiscard]] const std::vector<Processor>& cpus() const;
  [[nodiscard]] const std::vector<Processor>& gpus() const;
  [[nodiscard]] const std::vector<Processor>& omps() const;
  [[nodiscard]] const std::vector<Processor>& procs(TaskTarget target) const;

  [[nodiscard]] std::size_t total_cpu_count() const;
  [[nodiscard]] std::size_t total_gpu_count() const;
  [[nodiscard]] std::size_t total_omp_count() const;

  [[nodiscard]] std::size_t total_frame_buffer_size() const;
  [[nodiscard]] std::size_t total_socket_memory_size() const;

  [[nodiscard]] bool has_cpus() const;
  [[nodiscard]] bool has_gpus() const;
  [[nodiscard]] bool has_omps() const;

  [[nodiscard]] bool has_socket_memory() const;

  [[nodiscard]] Memory get_memory(Processor proc, StoreTarget target) const;
  [[nodiscard]] Memory system_memory() const;
  [[nodiscard]] Memory zerocopy_memory() const;
  [[nodiscard]] const std::map<Processor, Memory>& frame_buffers() const;
  [[nodiscard]] const std::map<Processor, Memory>& socket_memories() const;

  [[nodiscard]] std::uint32_t g2c_multi_hop_bandwidth(Memory gpu_mem, Memory cpu_mem) const;

  [[nodiscard]] LocalProcessorRange slice(TaskTarget target,
                                          const Machine& machine,
                                          bool fallback_to_global = false) const;

  std::uint32_t node_id{};
  std::uint32_t total_nodes{};

 private:
  void init_g2c_multi_hop_bandwidth_();

  std::vector<Processor> cpus_{};
  std::vector<Processor> gpus_{};
  std::vector<Processor> omps_{};

  Memory system_memory_, zerocopy_memory_;
  std::map<Processor, Memory> frame_buffers_{};
  std::map<Processor, Memory> socket_memories_{};
  std::unordered_map<Memory, std::unordered_map<Memory, std::uint32_t>> g2c_multi_hop_bandwidth_{};
};

}  // namespace legate::mapping::detail

#include "core/mapping/detail/machine.inl"
