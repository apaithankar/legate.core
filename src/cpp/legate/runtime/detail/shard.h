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

#include <legate/mapping/machine.h>

#include <legion.h>

namespace legate::detail {

class Library;

void register_legate_core_sharding_functors(const detail::Library* core_library);

[[nodiscard]] Legion::ShardingID find_sharding_functor_by_projection_functor(
  Legion::ProjectionID proj_id);

void create_sharding_functor_using_projection(Legion::ShardingID shard_id,
                                              Legion::ProjectionID proj_id,
                                              const mapping::ProcessorRange& range);

}  // namespace legate::detail
