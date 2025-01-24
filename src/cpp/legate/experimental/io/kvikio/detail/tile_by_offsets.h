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

#pragma once

#include <legate/task/task.h>
#include <legate/task/task_context.h>
#include <legate/utilities/detail/core_ids.h>
#include <legate/utilities/typedefs.h>

namespace legate::experimental::io::kvikio::detail {

/**
 * @brief Read a tiled Legate store by offsets in a single file
 * Task signature:
 *   - scalars:
 *     - path: std::string
 *     - offsets: tuple of std::int64_t
 *   - outputs:
 *     - buffer: store (any dtype)
 *
 * NB: the store must be contigues. To make Legate in force this,
 *     set `policy.exact = true` in `Mapper::store_mappings()`.
 *
 */
class TileByOffsetsRead : public LegateTask<TileByOffsetsRead> {
 public:
  static constexpr auto TASK_ID =
    LocalTaskID{legate::detail::CoreTask::IO_KVIKIO_TILE_BY_OFFSETS_READ};

  static void cpu_variant(legate::TaskContext context);
  static void omp_variant(legate::TaskContext context);
  static void gpu_variant(legate::TaskContext context);
};

}  // namespace legate::experimental::io::kvikio::detail
