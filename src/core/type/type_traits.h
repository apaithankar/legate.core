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

#include "core/type/type_info.h"
#include "core/utilities/macros.h"

#include "legate_defines.h"

#include <climits>

#if LegateDefined(LEGATE_USE_CUDA)
#include <complex>
#endif

#ifdef LEGION_REDOP_COMPLEX
#ifdef LEGION_REDOP_HALF
#define COMPLEX_HALF
#endif
#include "mathtypes/complex.h"
#endif

#ifdef LEGION_REDOP_HALF
#include "mathtypes/half.h"
#endif

/**
 * @file
 * @brief Definitions for type traits in Legate
 */

namespace legate {

/**
 * @ingroup util
 * @brief A template constexpr that converts types to type codes
 */
template <class>
inline constexpr Type::Code type_code_of = Type::Code::NIL;
template <>
inline constexpr Type::Code type_code_of<__half> = Type::Code::FLOAT16;
template <>
inline constexpr Type::Code type_code_of<float> = Type::Code::FLOAT32;
template <>
inline constexpr Type::Code type_code_of<double> = Type::Code::FLOAT64;
template <>
inline constexpr Type::Code type_code_of<std::int8_t> = Type::Code::INT8;
template <>
inline constexpr Type::Code type_code_of<std::int16_t> = Type::Code::INT16;
template <>
inline constexpr Type::Code type_code_of<std::int32_t> = Type::Code::INT32;
template <>
inline constexpr Type::Code type_code_of<std::int64_t> = Type::Code::INT64;
template <>
inline constexpr Type::Code type_code_of<std::uint8_t> = Type::Code::UINT8;
template <>
inline constexpr Type::Code type_code_of<std::uint16_t> = Type::Code::UINT16;
template <>
inline constexpr Type::Code type_code_of<std::uint32_t> = Type::Code::UINT32;
template <>
inline constexpr Type::Code type_code_of<std::uint64_t> = Type::Code::UINT64;
template <>
inline constexpr Type::Code type_code_of<bool> = Type::Code::BOOL;
template <>
inline constexpr Type::Code type_code_of<complex<float>> = Type::Code::COMPLEX64;
template <>
inline constexpr Type::Code type_code_of<complex<double>> = Type::Code::COMPLEX128;
// When the CUDA build is off, complex<T> is an alias to std::complex<T>
#if LegateDefined(LEGATE_USE_CUDA)
template <>
inline constexpr Type::Code type_code_of<std::complex<float>> = Type::Code::COMPLEX64;
template <>
inline constexpr Type::Code type_code_of<std::complex<double>> = Type::Code::COMPLEX128;
#endif
template <>
inline constexpr Type::Code type_code_of<std::string> = Type::Code::STRING;

template <Type::Code CODE>
struct TypeOf {
  using type = void;
};
template <>
struct TypeOf<Type::Code::BOOL> {
  using type = bool;
};
template <>
struct TypeOf<Type::Code::INT8> {
  using type = std::int8_t;
};
template <>
struct TypeOf<Type::Code::INT16> {
  using type = std::int16_t;
};
template <>
struct TypeOf<Type::Code::INT32> {
  using type = std::int32_t;
};
template <>
struct TypeOf<Type::Code::INT64> {
  using type = std::int64_t;
};
template <>
struct TypeOf<Type::Code::UINT8> {
  using type = std::uint8_t;
};
template <>
struct TypeOf<Type::Code::UINT16> {
  using type = std::uint16_t;
};
template <>
struct TypeOf<Type::Code::UINT32> {
  using type = std::uint32_t;
};
template <>
struct TypeOf<Type::Code::UINT64> {
  using type = std::uint64_t;
};
template <>
struct TypeOf<Type::Code::FLOAT16> {
  using type = __half;
};
template <>
struct TypeOf<Type::Code::FLOAT32> {
  using type = float;
};
template <>
struct TypeOf<Type::Code::FLOAT64> {
  using type = double;
};
template <>
struct TypeOf<Type::Code::COMPLEX64> {
  using type = complex<float>;
};
template <>
struct TypeOf<Type::Code::COMPLEX128> {
  using type = complex<double>;
};
template <>
struct TypeOf<Type::Code::STRING> {
  using type = std::string;
};

/**
 * @ingroup util
 * @brief A template that converts type codes to types
 */
template <Type::Code CODE>
using type_of = typename TypeOf<CODE>::type;

/**
 * @ingroup util
 * @brief A predicate that holds if the type code is of an integral type
 */
template <Type::Code CODE>
struct is_integral : std::is_integral<type_of<CODE>> {};

/**
 * @ingroup util
 * @brief A predicate that holds if the type code is of a signed integral type
 */
template <Type::Code CODE>
struct is_signed : std::is_signed<type_of<CODE>> {};

template <>
struct is_signed<Type::Code::FLOAT16> : std::true_type {};

/**
 * @ingroup util
 * @brief A predicate that holds if the type code is of an unsigned integral type
 */
template <Type::Code CODE>
struct is_unsigned : std::is_unsigned<type_of<CODE>> {};

/**
 * @ingroup util
 * @brief A predicate that holds if the type code is of a floating point type
 */
template <Type::Code CODE>
struct is_floating_point : std::is_floating_point<type_of<CODE>> {};

template <>
struct is_floating_point<Type::Code::FLOAT16> : std::true_type {};

/**
 * @ingroup util
 * @brief A predicate that holds if the type code is of a complex type
 */
template <Type::Code CODE>
struct is_complex : std::false_type {};

template <>
struct is_complex<Type::Code::COMPLEX64> : std::true_type {};

template <>
struct is_complex<Type::Code::COMPLEX128> : std::true_type {};

/**
 * @ingroup util
 * @brief A predicate that holds if the type is one of the supported complex types
 */
template <typename T>
struct is_complex_type : std::false_type {};

template <>
struct is_complex_type<complex<float>> : std::true_type {};

template <>
struct is_complex_type<complex<double>> : std::true_type {};

// When the CUDA build is off, complex<T> is an alias to std::complex<T>
#if LegateDefined(LEGATE_USE_CUDA)
template <>
struct is_complex_type<std::complex<float>> : std::true_type {};

template <>
struct is_complex_type<std::complex<double>> : std::true_type {};
#endif

}  // namespace legate
