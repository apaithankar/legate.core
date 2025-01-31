/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved. SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include <legate.h>

#include <gtest/gtest.h>

#include <utilities/utilities.h>

namespace span_test {

// NOLINTBEGIN(readability-magic-numbers)

namespace {

using SpanUnit = DefaultFixture;

constexpr bool BOOL_VALUE            = true;
constexpr std::int8_t INT8_VALUE     = 10;
constexpr std::int16_t INT16_VALUE   = -1000;
constexpr std::int32_t INT32_VALUE   = 2700;
constexpr std::int64_t INT64_VALUE   = -28;
constexpr std::uint8_t UINT8_VALUE   = 128;
constexpr std::uint16_t UINT16_VALUE = 65535;
constexpr std::uint32_t UINT32_VALUE = 999;
constexpr std::uint64_t UINT64_VALUE = 100;
constexpr float FLOAT_VALUE          = 1.23F;
constexpr double DOUBLE_VALUE        = -4.567;
// NOLINTBEGIN(cert-err58-cpp)
const std::string STRING_VALUE = "123";
const __half FLOAT16_VALUE1(0.1F);
const __half FLOAT16_VALUE2(0.2F);
const __half FLOAT16_VALUE3(0.3F);
const complex<float> COMPLEX_FLOAT_VALUE1{0, 1};
const complex<float> COMPLEX_FLOAT_VALUE2{2, 3};
const complex<float> COMPLEX_FLOAT_VALUE3{4, 5};
const complex<double> COMPLEX_DOUBLE_VALUE1{6, 7};
const complex<double> COMPLEX_DOUBLE_VALUE2{8, 9};
const complex<double> COMPLEX_DOUBLE_VALUE3{10, 11};
// NOLINTEND(cert-err58-cpp)

constexpr std::uint32_t DATA_SIZE = 3;

template <typename T>
void create(const T& value1, const T& value2, const T& value3)
{
  const auto data = std::array<T, DATA_SIZE>{value1, value2, value3};
  const auto span = legate::Span<const T>{data};

  EXPECT_EQ(span.ptr(), data.data());
  EXPECT_EQ(span.size(), data.size());
  EXPECT_EQ(span.end() - span.begin(), data.size());
  for (auto& to_compare : span) {
    const auto i = std::distance(span.begin(), &to_compare);

    EXPECT_EQ(data.at(i), to_compare);
  }
  EXPECT_EQ(span[0], value1);
  EXPECT_EQ(span[DATA_SIZE - 1], value3);
}

}  // namespace

TEST_F(SpanUnit, Create)
{
  create(BOOL_VALUE, BOOL_VALUE, !BOOL_VALUE);
  create(
    INT8_VALUE, static_cast<std::int8_t>(INT8_VALUE + 1), static_cast<std::int8_t>(INT8_VALUE + 2));
  create(INT16_VALUE,
         static_cast<std::int16_t>(INT16_VALUE + 1),
         static_cast<std::int16_t>(INT16_VALUE + 2));
  create(INT32_VALUE,
         static_cast<std::int32_t>(INT32_VALUE + 1),
         static_cast<std::int32_t>(INT32_VALUE + 2));
  create(INT64_VALUE,
         static_cast<std::int64_t>(INT64_VALUE + 1),
         static_cast<std::int64_t>(INT64_VALUE + 2));
  create(UINT8_VALUE,
         static_cast<std::uint8_t>(UINT8_VALUE + 1),
         static_cast<std::uint8_t>(UINT8_VALUE + 2));
  create(UINT16_VALUE,
         static_cast<std::uint16_t>(UINT16_VALUE + 1),
         static_cast<std::uint16_t>(UINT16_VALUE + 2));
  create(UINT32_VALUE,
         static_cast<std::uint32_t>(UINT32_VALUE + 1),
         static_cast<std::uint32_t>(UINT32_VALUE + 2));
  create(UINT64_VALUE,
         static_cast<std::uint64_t>(UINT64_VALUE + 1),
         static_cast<std::uint64_t>(UINT64_VALUE + 2));
  create(FLOAT_VALUE, FLOAT_VALUE + 1.0F, FLOAT_VALUE + 2.0F);
  create(DOUBLE_VALUE, DOUBLE_VALUE + 1.0, DOUBLE_VALUE + 2.0);
  create(STRING_VALUE, STRING_VALUE + "1", STRING_VALUE + "2");
  create(FLOAT16_VALUE1, FLOAT16_VALUE2, FLOAT16_VALUE3);
  create(COMPLEX_FLOAT_VALUE1, COMPLEX_FLOAT_VALUE2, COMPLEX_FLOAT_VALUE3);
  create(COMPLEX_DOUBLE_VALUE1, COMPLEX_DOUBLE_VALUE2, COMPLEX_DOUBLE_VALUE3);
}

TEST_F(SpanUnit, Subspan)
{
  const auto data_vec        = std::vector<std::uint64_t>(DATA_SIZE, UINT64_VALUE);
  const auto* data           = data_vec.data();
  legate::Span span          = legate::Span<const std::uint64_t>{data, DATA_SIZE};
  const legate::Span subspan = span.subspan(DATA_SIZE - 1);

  EXPECT_EQ(subspan.ptr(), &data[DATA_SIZE - 1]);
  EXPECT_EQ(subspan.size(), 1);
  EXPECT_EQ(subspan.end() - subspan.begin(), 1);
  for (auto& to_compare : span) {
    EXPECT_EQ(UINT64_VALUE, to_compare);
  }
}

// NOLINTEND(readability-magic-numbers)

}  // namespace span_test
