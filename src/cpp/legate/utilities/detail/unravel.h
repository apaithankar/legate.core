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

#include <legate/cuda/cuda.h>
#include <legate/utilities/typedefs.h>

#include <array>
#include <cstdint>

namespace legate::detail {

// This helper class converts indices to multi-dimensional points
template <std::int32_t NDIM>
class Unravel {
 public:
  LEGATE_HOST_DEVICE explicit Unravel(const Rect<NDIM>& rect);

  LEGATE_HOST_DEVICE [[nodiscard]] std::uint64_t volume() const;

  LEGATE_HOST_DEVICE [[nodiscard]] bool empty() const;

  LEGATE_HOST_DEVICE [[nodiscard]] Point<NDIM> operator()(std::uint64_t index) const;

 private:
  Point<NDIM> low_{};
  // strides_[NDIM - 1] stores the total volume
  std::array<std::uint64_t, NDIM> strides_{};
};

}  // namespace legate::detail

#include <legate/utilities/detail/unravel.inl>
