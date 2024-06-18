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

#include "core/data/detail/transform.h"

#include <utility>

namespace legate::detail {

inline NonInvertibleTransformation::NonInvertibleTransformation(std::string error_message)
  : error_message_{std::move(error_message)}
{
}

inline const char* NonInvertibleTransformation::what() const noexcept
{
  return error_message_.c_str();
}

// ==========================================================================================

inline TransformStack::TransformStack(private_tag,
                                      std::unique_ptr<StoreTransform>&& transform,
                                      InternalSharedPtr<TransformStack> parent)
  : transform_{std::move(transform)},
    parent_{std::move(parent)},
    convertible_{transform_->is_convertible() && parent_->is_convertible()}
{
}

inline TransformStack::TransformStack(std::unique_ptr<StoreTransform>&& transform,
                                      const InternalSharedPtr<TransformStack>& parent)
  : TransformStack{private_tag{}, std::move(transform), parent}
{
}

inline TransformStack::TransformStack(std::unique_ptr<StoreTransform>&& transform,
                                      InternalSharedPtr<TransformStack>&& parent)
  : TransformStack{private_tag{}, std::move(transform), std::move(parent)}
{
}

inline bool TransformStack::is_convertible() const { return convertible_; }

inline bool TransformStack::identity() const { return nullptr == transform_; }

// ==========================================================================================

inline Shift::Shift(std::int32_t dim, std::int64_t offset) : dim_{dim}, offset_{offset} {}

// the shift transform makes no change on the store's dimensions
inline proj::SymbolicPoint Shift::invert(proj::SymbolicPoint point) const { return point; }

inline Restrictions Shift::convert(Restrictions restrictions, bool /*forbid_fake_dim*/) const
{
  return restrictions;
}

inline Restrictions Shift::invert(Restrictions restrictions) const { return restrictions; }

inline tuple<std::uint64_t> Shift::invert_color(tuple<std::uint64_t> color) const { return color; }

inline tuple<std::uint64_t> Shift::invert_extents(tuple<std::uint64_t> extents) const
{
  return extents;
}

inline std::int32_t Shift::target_ndim(std::int32_t source_ndim) const { return source_ndim; }

inline void Shift::find_imaginary_dims(std::vector<std::int32_t>&) const {}

inline bool Shift::is_convertible() const { return true; }

// ==========================================================================================

inline Promote::Promote(std::int32_t extra_dim, std::int64_t dim_size)
  : extra_dim_{extra_dim}, dim_size_{dim_size}
{
}

inline tuple<std::uint64_t> Promote::invert_color(tuple<std::uint64_t> color) const
{
  return invert_point(std::move(color));
}

inline std::int32_t Promote::target_ndim(std::int32_t source_ndim) const { return source_ndim - 1; }

inline bool Promote::is_convertible() const { return true; }

// ==========================================================================================

inline Project::Project(std::int32_t dim, std::int64_t coord) : dim_{dim}, coord_{coord} {}

inline std::int32_t Project::target_ndim(std::int32_t source_ndim) const { return source_ndim + 1; }

inline bool Project::is_convertible() const { return true; }

// ==========================================================================================

inline tuple<std::uint64_t> Transpose::invert_color(tuple<std::uint64_t> color) const
{
  return invert_point(std::move(color));
}

inline std::int32_t Transpose::target_ndim(std::int32_t source_ndim) const { return source_ndim; }

inline bool Transpose::is_convertible() const { return true; }

// ==========================================================================================

inline void Delinearize::find_imaginary_dims(std::vector<std::int32_t>&) const {}

inline bool Delinearize::is_convertible() const { return false; }

}  // namespace legate::detail
