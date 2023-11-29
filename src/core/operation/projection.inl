/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "core/runtime/detail/projection.h"

namespace legate {

inline SymbolicExpr::SymbolicExpr(int32_t dim, int32_t weight, int32_t offset)
  : dim_{dim}, weight_{weight}, offset_{offset}
{
}

inline SymbolicExpr::SymbolicExpr(int32_t dim) : SymbolicExpr{dim, 1} {}

inline int32_t SymbolicExpr::dim() const { return dim_; }

inline int32_t SymbolicExpr::weight() const { return weight_; }

inline int32_t SymbolicExpr::offset() const { return offset_; }

inline bool SymbolicExpr::is_identity(int32_t dim) const
{
  return this->dim() == dim && weight() == 1 && offset() == 0;
}

inline bool SymbolicExpr::operator==(const SymbolicExpr& other) const
{
  return dim() == other.dim() && weight() == other.weight() && offset() == other.offset();
}

inline bool SymbolicExpr::operator<(const SymbolicExpr& other) const
{
  return std::tie(dim_, weight_, offset_) < std::tie(other.dim_, other.weight_, other.offset_);
}

inline SymbolicExpr SymbolicExpr::operator*(int32_t other) const
{
  return {dim(), weight() * other, offset() * other};
}

inline SymbolicExpr SymbolicExpr::operator+(int32_t other) const
{
  return {dim(), weight(), offset() + other};
}

inline SymbolicExpr dimension(int32_t dim) { return SymbolicExpr{dim}; }

inline SymbolicExpr constant(int32_t value) { return SymbolicExpr{0, 0, value}; }

}  // namespace legate
