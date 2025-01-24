/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
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

#include <legate/mapping/array.h>

namespace legate::mapping {

template <std::int32_t DIM>
Rect<DIM> Array::shape() const
{
  static_assert(DIM <= LEGATE_MAX_DIM);
  return Rect<DIM>{domain()};
}

inline Array::Array(const detail::Array* impl) : impl_{impl} {}

}  // namespace legate::mapping
