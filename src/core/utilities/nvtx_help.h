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

#include "core/utilities/macros.h"

#include "legate_defines.h"

#if LEGATE_DEFINED(LEGATE_USE_CUDA)
#include <nvtx3/nvToolsExt.h>
#else
using nvtxRangeId_t = char;
// NOLINTBEGIN(readability-identifier-naming)
constexpr nvtxRangeId_t nvtxRangeStartA(const char*) noexcept { return 0; }
constexpr void nvtxRangeEnd(nvtxRangeId_t) noexcept {}
// NOLINTEND(readability-identifier-naming)
#endif

namespace legate::nvtx {

class Range {
 public:
  explicit Range(const char* message) noexcept : range_{nvtxRangeStartA(message)} {}

  Range()                        = delete;
  Range(const Range&)            = delete;
  Range& operator=(const Range&) = delete;
  Range(Range&&)                 = delete;
  Range& operator=(Range&&)      = delete;

  ~Range() noexcept { nvtxRangeEnd(range_); }

 private:
  nvtxRangeId_t range_;
};

}  // namespace legate::nvtx
