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

#include "core/mapping/detail/base_mapper.h"

namespace legate::mapping::detail {

inline Legion::Logger& BaseMapper::logger() { return logger_; }

inline const Legion::Logger& BaseMapper::logger() const { return logger_; }

inline const std::vector<Processor>& BaseMapper::cpus() const { return local_machine_.cpus(); }

inline const std::vector<Processor>& BaseMapper::gpus() const { return local_machine_.gpus(); }

inline const std::vector<Processor>& BaseMapper::omps() const { return local_machine_.omps(); }

inline std::uint32_t BaseMapper::total_nodes() const { return local_machine_.total_nodes; }

inline const char* BaseMapper::get_mapper_name() const { return mapper_name_.c_str(); }

inline Legion::Mapping::Mapper::MapperSyncModel BaseMapper::get_mapper_sync_model() const
{
  return SERIALIZED_REENTRANT_MAPPER_MODEL;
}

inline bool BaseMapper::request_valid_instances() const { return false; }

}  // namespace legate::mapping::detail
