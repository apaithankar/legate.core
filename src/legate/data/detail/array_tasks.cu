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

#include "legate/cuda/cuda.h"
#include "legate/data/detail/array_tasks.h"
#include "legate/task/task_context.h"

namespace legate::detail {

namespace {

LEGATE_DEVICE inline std::size_t global_tid_1d()
{
  return static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
}

template <typename DescAcc>
LEGATE_KERNEL void fixup_ranges(std::size_t desc_volume,
                                Point<1> desc_lo,
                                Point<1> vardata_lo,
                                DescAcc desc_acc)
{
  auto tid = global_tid_1d();
  if (tid >= desc_volume) {
    return;
  }
  auto& desc = desc_acc[desc_lo + tid];
  desc.lo += vardata_lo;
  desc.hi += vardata_lo;
}

}  // namespace

/*static*/ void FixupRanges::gpu_variant(legate::TaskContext context)
{
  if (context.get_task_index()[0] == 0) {
    return;
  }

  // TODO(wonchanl): We need to extend this to nested cases
  const auto num_outputs = context.num_outputs();
  auto stream            = context.get_task_stream();

  for (std::uint32_t i = 0; i < num_outputs; ++i) {
    const auto list_arr   = context.output(i).as_list_array();
    const auto desc       = list_arr.descriptor();
    const auto desc_shape = desc.shape<1>();
    if (desc_shape.empty()) {
      continue;
    }

    auto vardata_lo = list_arr.vardata().shape<1>().lo;
    auto desc_acc   = desc.data().read_write_accessor<Rect<1>, 1>();

    std::size_t desc_volume = desc_shape.volume();
    auto num_blocks = (desc_volume + LEGATE_THREADS_PER_BLOCK - 1) / LEGATE_THREADS_PER_BLOCK;
    fixup_ranges<<<num_blocks, LEGATE_THREADS_PER_BLOCK, 0, stream>>>(
      desc_volume, desc_shape.lo, vardata_lo, desc_acc);
  }
}
namespace {

template <typename RangesAcc, typename OffsetsAcc>
LEGATE_KERNEL void offsets_to_ranges(std::size_t offsets_volume,
                                     std::int64_t vardata_volume,
                                     Point<1> offsets_lo,
                                     Point<1> vardata_lo,
                                     RangesAcc ranges_acc,
                                     OffsetsAcc offsets_acc)
{
  auto tid = global_tid_1d();
  if (tid >= offsets_volume) {
    return;
  }
  auto p      = offsets_lo + tid;
  auto& range = ranges_acc[p];
  range.lo[0] = vardata_lo + offsets_acc[p];
  range.hi[0] = vardata_lo + (tid != offsets_volume - 1 ? offsets_acc[p + 1] : vardata_volume) - 1;
}

}  // namespace

/*static*/ void OffsetsToRanges::gpu_variant(legate::TaskContext context)
{
  auto offsets = context.input(0).data();
  auto vardata = context.input(1).data();
  auto ranges  = context.output(0).data();

  auto offsets_shape = offsets.shape<1>();
  LEGATE_CHECK(offsets_shape == ranges.shape<1>());

  if (offsets_shape.empty()) {
    return;
  }

  auto vardata_shape = vardata.shape<1>();
  auto vardata_lo    = vardata_shape.lo[0];

  auto offsets_acc = offsets.read_accessor<int32_t, 1>();
  auto ranges_acc  = ranges.write_accessor<Rect<1>, 1>();

  auto stream = context.get_task_stream();

  std::size_t offsets_volume = offsets_shape.volume();
  std::size_t vardata_volume = vardata_shape.volume();

  auto num_blocks = (offsets_volume + LEGATE_THREADS_PER_BLOCK - 1) / LEGATE_THREADS_PER_BLOCK;
  offsets_to_ranges<<<num_blocks, LEGATE_THREADS_PER_BLOCK, 0, stream>>>(
    offsets_volume, vardata_volume, offsets_shape.lo, vardata_shape.lo, ranges_acc, offsets_acc);
}

namespace {

template <typename OffsetsAcc, typename RangesAcc>
LEGATE_KERNEL void ranges_to_offsets(std::size_t ranges_volume,
                                     Point<1> ranges_lo,
                                     OffsetsAcc offsets_acc,
                                     RangesAcc ranges_acc)
{
  auto tid = global_tid_1d();
  if (tid >= ranges_volume) {
    return;
  }
  auto p         = ranges_lo + tid;
  offsets_acc[p] = ranges_acc[p].lo[0] - ranges_acc[ranges_lo].lo[0];
}

}  // namespace

/*static*/ void RangesToOffsets::gpu_variant(legate::TaskContext context)
{
  auto ranges  = context.input(0).data();
  auto offsets = context.output(0).data();

  auto ranges_shape = ranges.shape<1>();
  LEGATE_CHECK(ranges_shape == offsets.shape<1>());

  if (ranges_shape.empty()) {
    return;
  }

  auto ranges_acc  = ranges.read_accessor<Rect<1>, 1>();
  auto offsets_acc = offsets.write_accessor<int32_t, 1>();

  auto stream = context.get_task_stream();

  auto ranges_volume = ranges_shape.volume();
  auto num_blocks    = (ranges_volume + LEGATE_THREADS_PER_BLOCK - 1) / LEGATE_THREADS_PER_BLOCK;
  ranges_to_offsets<<<num_blocks, LEGATE_THREADS_PER_BLOCK, 0, stream>>>(
    ranges_volume, ranges_shape.lo, offsets_acc, ranges_acc);
}

}  // namespace legate::detail
