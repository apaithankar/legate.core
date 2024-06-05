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

#include "core/data/detail/scalar.h"

#include "core/utilities/detail/buffer_builder.h"
#include "core/utilities/detail/deserializer.h"

#include "utilities/utilities.h"

#include <gtest/gtest.h>

namespace scalar_test {

using ScalarUnit = DefaultFixture;

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
const __half FLOAT16_VALUE(0.1F);
const complex<float> COMPLEX_FLOAT_VALUE{0, 1};
const complex<double> COMPLEX_DOUBLE_VALUE{0, 1};
// NOLINTEND(cert-err58-cpp)
constexpr std::uint32_t DATA_SIZE = 10;

struct PaddingStructData {
  bool bool_data;
  std::int32_t int32_data;
  std::uint64_t uint64_data;
  bool operator==(const PaddingStructData& other) const
  {
    return bool_data == other.bool_data && int32_data == other.int32_data &&
           uint64_data == other.uint64_data;
  }
};

struct [[gnu::packed]] NoPaddingStructData {
  bool bool_data;
  std::int32_t int32_data;
  std::uint64_t uint64_data;
  bool operator==(const NoPaddingStructData& other) const
  {
    return bool_data == other.bool_data && int32_data == other.int32_data &&
           uint64_data == other.uint64_data;
  }
};

static_assert(sizeof(PaddingStructData) > sizeof(NoPaddingStructData));

template <std::int32_t DIM>
struct MultiDimRectStruct {
  std::int64_t lo[DIM];
  std::int64_t hi[DIM];
};

template <typename T>
void check_type(T value);
template <typename T>
void check_struct_type_scalar(T& struct_data, bool align);
template <std::int32_t DIM>
void check_point_scalar(const std::int64_t bounds[]);
template <std::int32_t DIM>
void check_rect_scalar(const std::int64_t lo[], const std::int64_t hi[]);
template <std::int32_t DIM>
void check_rect_bounds(const MultiDimRectStruct<DIM>& to_compare,
                       const MultiDimRectStruct<DIM>& expect);
void check_pack(const legate::Scalar& scalar);
template <std::int32_t DIM>
void check_pack_point_scalar();
template <std::int32_t DIM>
void check_pack_rect_scalar();

class ScalarUnitTestDeserializer
  : public legate::detail::BaseDeserializer<ScalarUnitTestDeserializer> {
 public:
  ScalarUnitTestDeserializer(const void* args, std::size_t arglen);

  using BaseDeserializer::unpack_impl;
};

ScalarUnitTestDeserializer::ScalarUnitTestDeserializer(const void* args, std::size_t arglen)
  : BaseDeserializer{args, arglen}
{
}

template <typename T>
void check_type(T value, legate::Type type)
{
  {
    const legate::Scalar scalar{value};

    EXPECT_EQ(scalar.type().code(), legate::type_code_of_v<T>);
    EXPECT_EQ(scalar.size(), sizeof(T));
    EXPECT_EQ(scalar.value<T>(), value);
    EXPECT_EQ(scalar.values<T>().size(), 1);
    EXPECT_NE(scalar.ptr(), nullptr);
  }

  {
    const legate::Scalar scalar{value, type};

    EXPECT_EQ(scalar.type().code(), type.code());
    EXPECT_EQ(scalar.size(), type.size());
    EXPECT_EQ(scalar.value<T>(), value);
    EXPECT_EQ(scalar.values<T>().size(), 1);
    EXPECT_NE(scalar.ptr(), nullptr);
  }
}

template <typename T>
void check_binary_scalar(T& value)
{
  const legate::Scalar scalar{value, legate::binary_type(sizeof(T))};

  EXPECT_EQ(scalar.type().size(), sizeof(T));
  EXPECT_NE(scalar.ptr(), nullptr);

  EXPECT_EQ(value, scalar.value<T>());
  auto actual = scalar.values<T>();
  EXPECT_EQ(actual.size(), 1);
  EXPECT_EQ(actual[0], value);
}

template <typename T>
void check_struct_type_scalar(T& struct_data, bool align)
{
  const legate::Scalar scalar{
    struct_data, legate::struct_type(align, legate::bool_(), legate::int32(), legate::uint64())};

  EXPECT_EQ(scalar.type().code(), legate::Type::Code::STRUCT);
  EXPECT_EQ(scalar.size(), sizeof(T));
  EXPECT_NE(scalar.ptr(), nullptr);

  // Check value
  T actual_data = scalar.value<T>();
  // When taking the address (or reference!) of a packed struct member, the resulting
  // pointer/reference is _not_ aligned to its normal type. This is a problem when other
  // functions (like EXPECT_EQ()) take their arguments by reference.
  //
  // So we need to make copies of the values (which will be properly aligned since they are
  // locals), in order for this not to raise UBSAN errors.
  auto compare = [](auto lhs, auto rhs) { EXPECT_EQ(lhs, rhs); };

  compare(actual_data.bool_data, struct_data.bool_data);
  compare(actual_data.int32_data, struct_data.int32_data);
  compare(actual_data.uint64_data, struct_data.uint64_data);

  // Check values
  const legate::Span actual_values{scalar.values<T>()};
  const auto expected_values = legate::Span<const T>{&struct_data, 1};

  EXPECT_EQ(actual_values.size(), expected_values.size());
  compare(actual_values.begin()->bool_data, expected_values.begin()->bool_data);
  compare(actual_values.begin()->int32_data, expected_values.begin()->int32_data);
  compare(actual_values.begin()->uint64_data, expected_values.begin()->uint64_data);
  EXPECT_NE(actual_values.ptr(), expected_values.ptr());
}

template <std::int32_t DIM>
void check_point_scalar(const std::int64_t bounds[])
{
  const auto point = legate::Point<DIM>{bounds};
  const legate::Scalar scalar{point};

  EXPECT_EQ(scalar.type().code(), legate::Type::Code::FIXED_ARRAY);
  auto fixed_type = legate::fixed_array_type(legate::int64(), DIM);
  EXPECT_EQ(scalar.size(), fixed_type.size());
  EXPECT_NE(scalar.ptr(), nullptr);

  // Check values
  const legate::Span expected_values = legate::Span<const std::int64_t>{bounds, DIM};
  const legate::Span actual_values{scalar.values<std::int64_t>()};

  for (int i = 0; i < DIM; i++) {
    EXPECT_EQ(actual_values[i], expected_values[i]);
  }
  EXPECT_EQ(actual_values.size(), DIM);
  EXPECT_EQ(actual_values.size(), expected_values.size());

  // Invalid type
  EXPECT_THROW(static_cast<void>(scalar.values<std::int32_t>()), std::invalid_argument);
}

template <std::int32_t DIM>
void check_rect_scalar(const std::int64_t lo[], const std::int64_t hi[])
{
  auto point_lo = legate::Point<DIM>{lo};
  auto point_hi = legate::Point<DIM>{hi};
  auto rect     = legate::Rect<DIM>{point_lo, point_hi};
  const legate::Scalar scalar{rect};

  EXPECT_EQ(scalar.type().code(), legate::Type::Code::STRUCT);
  auto struct_type = legate::struct_type(true,
                                         legate::fixed_array_type(legate::int64(), DIM),
                                         legate::fixed_array_type(legate::int64(), DIM));
  EXPECT_EQ(scalar.size(), struct_type.size());
  EXPECT_NE(scalar.ptr(), nullptr);

  // Check values
  auto actual_data = scalar.value<MultiDimRectStruct<DIM>>();
  MultiDimRectStruct<DIM> expected_data;

  for (int i = 0; i < DIM; i++) {
    expected_data.lo[i] = lo[i];
    expected_data.hi[i] = hi[i];
  }
  check_rect_bounds(actual_data, expected_data);

  const legate::Span actual_values{scalar.values<MultiDimRectStruct<DIM>>()};
  const legate::Span expected_values =
    legate::Span<const MultiDimRectStruct<DIM>>{&expected_data, 1};

  EXPECT_EQ(actual_values.size(), 1);
  EXPECT_EQ(actual_values.size(), expected_values.size());
  check_rect_bounds(*actual_values.begin(), *expected_values.begin());
  EXPECT_NE(actual_values.ptr(), expected_values.ptr());

  // Invalid type
  EXPECT_THROW(static_cast<void>(scalar.value<std::int32_t>()), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(scalar.values<std::uint8_t>()), std::invalid_argument);
}

template <std::int32_t DIM>
void check_rect_bounds(const MultiDimRectStruct<DIM>& to_compare,
                       const MultiDimRectStruct<DIM>& expect)
{
  EXPECT_EQ(std::size(to_compare.lo), std::size(to_compare.hi));
  EXPECT_EQ(std::size(expect.lo), std::size(expect.hi));
  EXPECT_EQ(std::size(to_compare.lo), std::size(expect.lo));

  for (std::size_t i = 0; i < std::size(to_compare.lo); i++) {
    EXPECT_EQ(to_compare.lo[i], expect.lo[i]);
    EXPECT_EQ(to_compare.hi[i], expect.hi[i]);
  }
}

void check_pack(const legate::Scalar& scalar)
{
  legate::detail::BufferBuilder buf;
  scalar.impl()->pack(buf);
  auto legion_buffer = buf.to_legion_buffer();
  EXPECT_NE(legion_buffer.get_ptr(), nullptr);
  legate::detail::BaseDeserializer<ScalarUnitTestDeserializer> deserializer{
    legion_buffer.get_ptr(), legion_buffer.get_size()};
  auto scalar_unpack = deserializer.unpack_scalar();
  EXPECT_EQ(scalar_unpack->type()->code, scalar.type().code());
  EXPECT_EQ(scalar_unpack->size(), scalar.size());
}

template <std::int32_t DIM>
void check_pack_point_scalar()
{
  auto point = legate::Point<DIM>::ONES();
  const legate::Scalar scalar{point};

  check_pack(scalar);
}

template <std::int32_t DIM>
void check_pack_rect_scalar()
{
  auto rect = legate::Rect<DIM>{legate::Point<DIM>::ZEROES(), legate::Point<DIM>::ONES()};
  const legate::Scalar scalar{rect};

  check_pack(scalar);
}

TEST_F(ScalarUnit, CreateWithObject)
{
  // constructor with Scalar object
  const legate::Scalar scalar1{INT32_VALUE};
  const legate::Scalar scalar2{scalar1};  // NOLINT(performance-unnecessary-copy-initialization)

  EXPECT_EQ(scalar2.type().code(), scalar1.type().code());
  EXPECT_EQ(scalar2.size(), scalar1.size());
  EXPECT_EQ(scalar2.size(), legate::uint32().size());
  EXPECT_NE(scalar2.ptr(), nullptr);
  EXPECT_EQ(scalar2.value<std::int32_t>(), scalar1.value<std::int32_t>());
  EXPECT_EQ(scalar2.values<std::int32_t>().size(), scalar1.values<std::int32_t>().size());
  EXPECT_EQ(*scalar2.values<std::int32_t>().begin(), *scalar1.values<std::int32_t>().begin());

  // Invalid type
  EXPECT_THROW(static_cast<void>(scalar2.value<std::int64_t>()), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(scalar2.values<std::int8_t>()), std::invalid_argument);
}

TEST_F(ScalarUnit, CreateSharedScalar)
{
  // unowned
  {
    const auto data_vec = std::vector<std::uint64_t>(DATA_SIZE, UINT64_VALUE);
    const auto* data    = data_vec.data();
    const legate::Scalar scalar{legate::uint64(), data};

    EXPECT_EQ(scalar.type().code(), legate::Type::Code::UINT64);
    EXPECT_EQ(scalar.size(), legate::uint64().size());
    EXPECT_EQ(scalar.ptr(), data);

    EXPECT_EQ(scalar.value<std::uint64_t>(), UINT64_VALUE);

    const legate::Span actual_values{scalar.values<std::uint64_t>()};
    const legate::Span expected_values = legate::Span<const std::uint64_t>{data, 1};

    EXPECT_EQ(*actual_values.begin(), *expected_values.begin());
    EXPECT_EQ(actual_values.size(), expected_values.size());

    // Invalid type
    EXPECT_THROW(static_cast<void>(scalar.value<std::int32_t>()), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(scalar.values<std::int8_t>()), std::invalid_argument);
  }

  // owned
  {
    const auto data_vec = std::vector<std::uint32_t>(DATA_SIZE, UINT32_VALUE);
    const auto* data    = data_vec.data();
    const legate::Scalar scalar{legate::uint32(), data, true};

    EXPECT_NE(scalar.ptr(), data);
  }
}

TEST_F(ScalarUnit, CreateWithValue)
{
  check_type(BOOL_VALUE, legate::bool_());
  check_type(INT8_VALUE, legate::int8());
  check_type(INT16_VALUE, legate::int16());
  check_type(INT32_VALUE, legate::int32());
  check_type(INT64_VALUE, legate::int64());
  check_type(UINT8_VALUE, legate::uint8());
  check_type(UINT16_VALUE, legate::uint16());
  check_type(UINT32_VALUE, legate::uint32());
  check_type(UINT64_VALUE, legate::uint64());
  check_type(FLOAT_VALUE, legate::float32());
  check_type(DOUBLE_VALUE, legate::float64());
  check_type(FLOAT16_VALUE, legate::float16());
  check_type(COMPLEX_FLOAT_VALUE, legate::complex64());
  check_type(COMPLEX_DOUBLE_VALUE, legate::complex128());

  // Size mismatch
  EXPECT_THROW(legate::Scalar(FLOAT16_VALUE, legate::float32()), std::invalid_argument);
}

TEST_F(ScalarUnit, CreateBinary)
{
  {
    PaddingStructData value = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    check_binary_scalar(value);
  }

  {
    NoPaddingStructData value = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    check_binary_scalar(value);
  }
}

TEST_F(ScalarUnit, CreateWithVector)
{
  // constructor with arrays
  const std::vector<std::int32_t> scalar_data{INT32_VALUE, INT32_VALUE};
  const legate::Scalar scalar{scalar_data};

  EXPECT_EQ(scalar.type().code(), legate::Type::Code::FIXED_ARRAY);

  auto fixed_type = legate::fixed_array_type(legate::int32(), scalar_data.size());

  EXPECT_EQ(scalar.size(), fixed_type.size());

  // check values here. Note: no value allowed for a fixed arrays scalar
  const std::vector<std::int32_t> data_vec = {INT32_VALUE, INT32_VALUE};
  const auto* data                         = data_vec.data();
  const legate::Span expected_values = legate::Span<const std::int32_t>{data, scalar_data.size()};
  const legate::Span actual_values{scalar.values<std::int32_t>()};

  for (std::size_t i = 0; i < scalar_data.size(); i++) {
    EXPECT_EQ(actual_values[i], expected_values[i]);
  }
  EXPECT_EQ(actual_values.size(), expected_values.size());

  // Invalid type
  EXPECT_THROW(static_cast<void>(scalar.value<std::int32_t>()), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(scalar.values<std::string>()), std::invalid_argument);
}

TEST_F(ScalarUnit, CreateWithString)
{
  // constructor with string
  auto input_string = STRING_VALUE;
  const legate::Scalar scalar{input_string};

  EXPECT_EQ(scalar.type().code(), legate::Type::Code::STRING);

  auto expected_size = sizeof(uint32_t) + sizeof(char) * input_string.size();

  EXPECT_EQ(scalar.size(), expected_size);
  EXPECT_NE(scalar.ptr(), input_string.data());

  // Check values
  EXPECT_EQ(scalar.value<std::string>(), input_string);
  EXPECT_EQ(scalar.value<std::string_view>(), input_string);

  const legate::Span actual_values{scalar.values<char>()};

  EXPECT_EQ(actual_values.size(), input_string.size());
  EXPECT_EQ(*actual_values.begin(), input_string[0]);

  // Invalid type
  EXPECT_THROW(static_cast<void>(scalar.value<std::int32_t>()), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(scalar.values<std::int32_t>()), std::invalid_argument);
}

TEST_F(ScalarUnit, CreateWithStructType)
{
  // with struct padding
  {
    PaddingStructData struct_data = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    check_struct_type_scalar(struct_data, true);
  }

  // without struct padding
  {
    NoPaddingStructData struct_data = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    check_struct_type_scalar(struct_data, false);
  }
}

TEST_F(ScalarUnit, CreateWithPoint)
{
  {
    constexpr std::int64_t bounds[] = {0};
    check_point_scalar<1>(bounds);
  }

  {
    constexpr std::int64_t bounds[] = {1, 8};
    check_point_scalar<2>(bounds);
  }

  {
    constexpr std::int64_t bounds[] = {-10, 2, -1};
    check_point_scalar<3>(bounds);
  }

  {
    constexpr std::int64_t bounds[] = {1, 5, 7, 200};
    check_point_scalar<4>(bounds);
  }

  // Invalid dim
  EXPECT_THROW(legate::Scalar{legate::Point<10>::ONES()}, std::out_of_range);
}

TEST_F(ScalarUnit, CreateWithRect)
{
  {
    constexpr std::int64_t lo[] = {1};
    constexpr std::int64_t hi[] = {-9};

    check_rect_scalar<1>(lo, hi);
  }

  {
    constexpr std::int64_t lo[] = {-1, 3};
    constexpr std::int64_t hi[] = {9, -2};

    check_rect_scalar<2>(lo, hi);
  }

  {
    constexpr std::int64_t lo[] = {0, 1, 2};
    constexpr std::int64_t hi[] = {3, 4, 5};

    check_rect_scalar<3>(lo, hi);
  }

  {
    constexpr std::int64_t lo[] = {-5, 1, -7, 10};
    constexpr std::int64_t hi[] = {4, 5, 6, 7};

    check_rect_scalar<4>(lo, hi);
  }
}

TEST_F(ScalarUnit, OperatorEqual)
{
  const legate::Scalar scalar1{INT32_VALUE};
  legate::Scalar scalar2{UINT64_VALUE};

  scalar2 = scalar1;
  EXPECT_EQ(scalar2.type().code(), scalar1.type().code());
  EXPECT_EQ(scalar2.size(), scalar2.size());
  EXPECT_EQ(scalar2.value<std::int32_t>(), scalar1.value<std::int32_t>());
  EXPECT_EQ(scalar2.values<std::int32_t>().size(), scalar1.values<std::int32_t>().size());
}

TEST_F(ScalarUnit, Pack)
{
  // test pack for a fixed array scalar
  {
    check_pack(legate::Scalar{});
  }

  // test pack for a fixed array scalar
  {
    const legate::Scalar scalar{std::vector<std::uint32_t>{UINT32_VALUE, UINT32_VALUE}};

    check_pack(scalar);
  }

  // test pack for a single value scalar
  {
    const legate::Scalar scalar{BOOL_VALUE};

    check_pack(scalar);
  }

  // test pack for string scalar
  {
    const legate::Scalar scalar{STRING_VALUE};

    check_pack(scalar);
  }

  // test pack for padding struct type scalar
  {
    const PaddingStructData struct_data = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    const legate::Scalar scalar{
      struct_data, legate::struct_type(true, legate::bool_(), legate::int32(), legate::uint64())};

    check_pack(scalar);
  }

  // test pack for no padding struct type scalar
  {
    const NoPaddingStructData struct_data = {BOOL_VALUE, INT32_VALUE, UINT64_VALUE};
    const legate::Scalar scalar{
      struct_data, legate::struct_type(false, legate::bool_(), legate::int32(), legate::uint64())};

    check_pack(scalar);
  }

  // test pack for a shared scalar
  {
    const auto data_vec = std::vector<std::uint64_t>(DATA_SIZE, UINT64_VALUE);
    const auto* data    = data_vec.data();
    const legate::Scalar scalar{legate::uint64(), data};

    check_pack(scalar);
  }

  // test pack for scalar created by another scalar
  {
    const legate::Scalar scalar1{INT32_VALUE};
    const legate::Scalar scalar2{scalar1};  // NOLINT(performance-unnecessary-copy-initialization)

    check_pack(scalar2);
  }

  // test pack for scalar created by point
  {
    check_pack_point_scalar<1>();
    check_pack_point_scalar<2>();
    check_pack_point_scalar<3>();
    check_pack_point_scalar<4>();
  }

  // test pack for scalar created by rect
  {
    check_pack_rect_scalar<1>();
    check_pack_rect_scalar<2>();
    check_pack_rect_scalar<3>();
    check_pack_rect_scalar<4>();
  }
}
}  // namespace scalar_test
