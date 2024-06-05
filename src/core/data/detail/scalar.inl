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

#include "core/data/detail/scalar.h"
#include "core/type/type_traits.h"

#include <utility>

namespace legate::detail {

inline Scalar::Scalar(InternalSharedPtr<Type> type)
  : own_{true}, type_{std::move(type)}, data_{new char[type_->size()]{}}
{
}

template <typename T>
inline Scalar::Scalar(T value)
  : own_{true},
    type_{detail::primitive_type(type_code_of_v<T>)},
    data_{copy_data_(std::addressof(value), sizeof(T))}
{
  static_assert(type_code_of_v<T> != Type::Code::FIXED_ARRAY);
  static_assert(type_code_of_v<T> != Type::Code::STRUCT);
  static_assert(type_code_of_v<T> != Type::Code::STRING);
  static_assert(type_code_of_v<T> != Type::Code::NIL);
}

inline Scalar::~Scalar() { clear_data_(); }

inline Scalar::Scalar(const Scalar& other)
  : own_{other.own_},
    type_{other.type_},
    data_{other.own_ ? copy_data_(other.data_, other.size()) : other.data_}
{
}

inline Scalar::Scalar(Scalar&& other) noexcept
  : own_{std::exchange(other.own_, false)},
    type_{std::move(other.type_)},
    data_{std::exchange(other.data_, nullptr)}
{
}

inline const InternalSharedPtr<Type>& Scalar::type() const { return type_; }

inline const void* Scalar::data() const { return data_; }

}  // namespace legate::detail
