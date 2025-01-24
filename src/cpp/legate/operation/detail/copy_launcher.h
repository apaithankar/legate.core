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

#include <legate/mapping/detail/machine.h>
#include <legate/operation/detail/launcher_arg.h>
#include <legate/operation/detail/store_projection.h>
#include <legate/utilities/detail/core_ids.h>

#include <memory>
#include <vector>

namespace legate {
class Scalar;
}  // namespace legate

namespace legate::detail {

class LogicalStore;
class OutputRequirementAnalyzer;
class RequirementAnalyzer;
class BufferBuilder;

class CopyArg final : public Serializable {
 public:
  CopyArg(std::uint32_t req_idx,
          LogicalStore* store,
          Legion::FieldID field_id,
          Legion::PrivilegeMode privilege,
          std::unique_ptr<StoreProjection> store_proj);

  void pack(BufferBuilder& buffer) const override;

  template <bool SINGLE>
  void populate_requirement(Legion::RegionRequirement& requirement);

 private:
  std::uint32_t req_idx_{};
  LogicalStore* store_{};
  Legion::LogicalRegion region_{};
  Legion::FieldID field_id_{};
  Legion::PrivilegeMode privilege_{};
  std::unique_ptr<StoreProjection> store_proj_{};
};

class CopyLauncher {
 public:
  CopyLauncher(const mapping::detail::Machine& machine,
               std::int32_t priority,
               std::int64_t tag = 0);

  void add_input(const InternalSharedPtr<LogicalStore>& store,
                 std::unique_ptr<StoreProjection> store_proj);
  void add_output(const InternalSharedPtr<LogicalStore>& store,
                  std::unique_ptr<StoreProjection> store_proj);
  void add_inout(const InternalSharedPtr<LogicalStore>& store,
                 std::unique_ptr<StoreProjection> store_proj);
  void add_reduction(const InternalSharedPtr<LogicalStore>& store,
                     std::unique_ptr<StoreProjection> store_proj);
  void add_source_indirect(const InternalSharedPtr<LogicalStore>& store,
                           std::unique_ptr<StoreProjection> store_proj);
  void add_target_indirect(const InternalSharedPtr<LogicalStore>& store,
                           std::unique_ptr<StoreProjection> store_proj);

  void add_store(std::vector<CopyArg>& args,
                 const InternalSharedPtr<LogicalStore>& store,
                 std::unique_ptr<StoreProjection> store_proj,
                 Legion::PrivilegeMode privilege);

  void set_source_indirect_out_of_range(bool flag) { source_indirect_out_of_range_ = flag; }
  void set_target_indirect_out_of_range(bool flag) { target_indirect_out_of_range_ = flag; }

  void execute(const Legion::Domain& launch_domain);
  void execute_single();

  void pack_args(BufferBuilder& buffer);
  void pack_sharding_functor_id(BufferBuilder& buffer);

 private:
  template <typename Launcher>
  void populate_copy_(Launcher& launcher);

  const mapping::detail::Machine& machine_;
  std::int32_t priority_{static_cast<std::int32_t>(TaskPriority::DEFAULT)};
  std::int64_t tag_{};
  Legion::ProjectionID key_proj_id_{};

  std::vector<CopyArg> inputs_{};
  std::vector<CopyArg> outputs_{};
  std::vector<CopyArg> source_indirect_{};
  std::vector<CopyArg> target_indirect_{};

  bool source_indirect_out_of_range_{true};
  bool target_indirect_out_of_range_{true};
};

}  // namespace legate::detail

#include <legate/operation/detail/copy_launcher.inl>
