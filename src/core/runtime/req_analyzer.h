/* Copyright 2022 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#pragma once

#include <tuple>
#include "legate.h"

namespace legate {

class Projection {
 public:
  Projection() {}
  Projection(Legion::LogicalPartition partition, Legion::ProjectionID proj_id);

 public:
  void set_reduction_op(Legion::ReductionOpID redop);

 public:
  const Legion::LogicalPartition partition{Legion::LogicalPartition::NO_PART};
  const Legion::ProjectionID proj_id{0};
  // TODO: Make this const as well
  Legion::ReductionOpID redop{-1};
};

class ProjectionInfo {
 public:
  ProjectionInfo(const Projection* proj, Legion::MappingTagID tag, Legion::RegionFlags flags);

 public:
  ProjectionInfo(const ProjectionInfo&)            = default;
  ProjectionInfo& operator=(const ProjectionInfo&) = default;

 public:
  bool operator<(const ProjectionInfo& other) const;
  bool operator==(const ProjectionInfo& other) const;

 public:
  void populate_launcher(Legion::TaskLauncher* task,
                         const Legion::LogicalRegion& region,
                         const std::vector<Legion::FieldID>& fields,
                         Legion::PrivilegeMode privilege) const;
  void populate_launcher(Legion::IndexTaskLauncher* task,
                         const Legion::LogicalRegion& region,
                         const std::vector<Legion::FieldID>& fields,
                         Legion::PrivilegeMode privilege) const;

 public:
  Legion::LogicalPartition partition;
  Legion::ProjectionID proj_id;
  Legion::ReductionOpID redop;
  Legion::MappingTagID tag;
  Legion::RegionFlags flags;
};

class ProjectionSet {
 public:
  void insert(Legion::PrivilegeMode new_privilege, const ProjectionInfo* proj_info);

 public:
  Legion::PrivilegeMode privilege;
  std::set<ProjectionInfo> proj_infos;
};

class FieldSet {
 public:
  using Key = std::pair<Legion::PrivilegeMode, ProjectionInfo>;

 public:
  void insert(Legion::FieldID field_id,
              Legion::PrivilegeMode privilege,
              const ProjectionInfo* proj_info);
  uint32_t num_requirements() const;
  uint32_t get_requirement_index(Legion::PrivilegeMode privilege,
                                 const ProjectionInfo* proj_info) const;

 public:
  void coalesce();
  void populate_launcher(Legion::IndexTaskLauncher* task,
                         const Legion::LogicalRegion& region) const;
  void populate_launcher(Legion::TaskLauncher* task, const Legion::LogicalRegion& region) const;

 private:
  std::map<Key, std::vector<Legion::FieldID>> coalesced_;
  std::map<Key, uint32_t> req_indices_;

 private:
  std::map<Legion::FieldID, ProjectionSet> field_projs_;
};

class RequirementAnalyzer {
 public:
  ~RequirementAnalyzer();

 public:
  void insert(const Legion::LogicalRegion& region,
              Legion::FieldID field_id,
              Legion::PrivilegeMode privilege,
              const ProjectionInfo* proj_info);
  uint32_t get_requirement_index(const Legion::LogicalRegion& region,
                                 Legion::PrivilegeMode privilege,
                                 const ProjectionInfo* proj_info) const;

 public:
  void analyze_requirements();
  void populate_launcher(Legion::IndexTaskLauncher* task) const;
  void populate_launcher(Legion::TaskLauncher* task) const;

 private:
  std::map<Legion::LogicalRegion, std::pair<FieldSet, uint32_t>> field_sets_;
};

}  // namespace legate
