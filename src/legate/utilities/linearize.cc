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

#include "legate/utilities/linearize.h"

#include "legate/utilities/dispatch.h"

namespace legate {

namespace {

class LinearizeFn {
 public:
  template <std::int32_t DIM>
  [[nodiscard]] std::size_t operator()(const DomainPoint& lo_dp,
                                       const DomainPoint& hi_dp,
                                       const DomainPoint& point_dp) const
  {
    const Point<DIM> lo      = lo_dp;
    const Point<DIM> hi      = hi_dp;
    const Point<DIM> point   = point_dp;
    const Point<DIM> extents = hi - lo + Point<DIM>::ONES();
    std::size_t idx          = 0;

    for (std::int32_t dim = 0; dim < DIM; ++dim) {
      idx = idx * extents[dim] + point[dim] - lo[dim];
    }
    return idx;
  }
};

}  // namespace

std::size_t linearize(const DomainPoint& lo, const DomainPoint& hi, const DomainPoint& point)
{
  return dim_dispatch(point.dim, LinearizeFn{}, lo, hi, point);
}

namespace {

class DelinearizeFn {
 public:
  template <std::int32_t DIM>
  [[nodiscard]] DomainPoint operator()(const DomainPoint& lo_dp,
                                       const DomainPoint& hi_dp,
                                       std::size_t idx) const
  {
    const Point<DIM> lo      = lo_dp;
    const Point<DIM> hi      = hi_dp;
    const Point<DIM> extents = hi - lo + Point<DIM>::ONES();
    Point<DIM> point;

    for (std::int32_t dim = DIM - 1; dim >= 0; --dim) {
      point[dim] = idx % extents[dim] + lo[dim];
      idx /= extents[dim];
    }
    return point;
  }
};

}  // namespace

DomainPoint delinearize(const DomainPoint& lo, const DomainPoint& hi, std::size_t idx)
{
  return dim_dispatch(lo.dim, DelinearizeFn{}, lo, hi, idx);
}

}  // namespace legate
