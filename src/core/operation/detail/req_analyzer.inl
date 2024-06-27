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

#include "core/operation/detail/req_analyzer.h"

namespace legate::detail {

inline bool RequirementAnalyzer::empty() const { return field_sets_.empty(); }

inline void RequirementAnalyzer::relax_interference_checks(bool relax)
{
  relax_interference_checks_ = relax;
}

// ==========================================================================================

inline bool OutputRequirementAnalyzer::empty() const { return field_groups_.empty(); }

// ==========================================================================================

inline void FutureAnalyzer::insert(Legion::Future future) { futures_.push_back(std::move(future)); }

inline void FutureAnalyzer::insert(Legion::FutureMap future_map)
{
  future_maps_.push_back(std::move(future_map));
}

inline std::int32_t FutureAnalyzer::get_index(const Legion::Future& future) const
{
  return future_indices_.at(future);
}

inline std::int32_t FutureAnalyzer::get_index(const Legion::FutureMap& future_map) const
{
  return future_map_indices_.at(future_map);
}

// ==========================================================================================

inline void StoreAnalyzer::insert(const InternalSharedPtr<LogicalRegionField>& region_field,
                                  Legion::PrivilegeMode privilege,
                                  const StoreProjection& store_proj)
{
  req_analyzer_.insert(region_field->region(), region_field->field_id(), privilege, store_proj);
}

inline void StoreAnalyzer::insert(std::uint32_t dim,
                                  const Legion::FieldSpace& field_space,
                                  Legion::FieldID field_id)
{
  out_analyzer_.insert(dim, field_space, field_id);
}

inline void StoreAnalyzer::insert(Legion::Future future)
{
  fut_analyzer_.insert(std::move(future));
}

inline void StoreAnalyzer::insert(Legion::FutureMap future_map)
{
  fut_analyzer_.insert(std::move(future_map));
}

inline void StoreAnalyzer::analyze()
{
  req_analyzer_.analyze_requirements();
  out_analyzer_.analyze_requirements();
  fut_analyzer_.analyze_futures();
}

inline std::uint32_t StoreAnalyzer::get_index(const Legion::LogicalRegion& region,
                                              Legion::PrivilegeMode privilege,
                                              const StoreProjection& store_proj,
                                              Legion::FieldID field_id) const
{
  return req_analyzer_.get_requirement_index(region, privilege, store_proj, field_id);
}

inline std::uint32_t StoreAnalyzer::get_index(const Legion::FieldSpace& field_space,
                                              Legion::FieldID field_id) const
{
  return out_analyzer_.get_requirement_index(field_space, field_id);
}

inline std::int32_t StoreAnalyzer::get_index(const Legion::Future& future) const
{
  return fut_analyzer_.get_index(future);
}

inline std::int32_t StoreAnalyzer::get_index(const Legion::FutureMap& future_map) const
{
  return fut_analyzer_.get_index(future_map);
}

template <typename Launcher>
inline void StoreAnalyzer::populate(Launcher& launcher,
                                    std::vector<Legion::OutputRequirement>& out_reqs)
{
  req_analyzer_.populate_launcher(launcher);
  out_analyzer_.populate_output_requirements(out_reqs);
  fut_analyzer_.populate_launcher(launcher);
}

inline bool StoreAnalyzer::can_be_local_function_task() const
{
  return req_analyzer_.empty() && out_analyzer_.empty();
}

inline void StoreAnalyzer::relax_interference_checks(bool relax)
{
  req_analyzer_.relax_interference_checks(relax);
}

}  // namespace legate::detail
