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

#include "legate.h"
#include "utilities/utilities.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

namespace logicalstore_unit {

namespace {

using LogicalStoreUnit = DefaultFixture;

// NOLINTBEGIN(readability-magic-numbers)

template <typename T>
void test_unbound_store(std::int32_t dim)
{
  auto runtime = legate::Runtime::get_runtime();
  // primitive type
  {
    const legate::Type type{legate::primitive_type(legate::type_code_of_v<T>)};
    auto store = runtime->create_store(type, dim);

    EXPECT_TRUE(store.unbound());
    EXPECT_EQ(store.dim(), dim);
    EXPECT_THROW(static_cast<void>(store.extents()), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(store.volume()), std::invalid_argument);
    EXPECT_EQ(store.type().code(), legate::type_code_of_v<T>);
    EXPECT_FALSE(store.transformed());
    EXPECT_FALSE(store.has_scalar_storage());
  }

  const auto TYPES = {
    legate::binary_type(10),
    static_cast<legate::Type>(legate::fixed_array_type(legate::bool_(), dim)),
    static_cast<legate::Type>(legate::struct_type(false, legate::bool_(), legate::uint32())),
    static_cast<legate::Type>(
      legate::struct_type(true, legate::int16(), legate::bool_(), legate::uint32()))};

  for (const auto& type : TYPES) {
    auto store = runtime->create_store(type, dim);

    EXPECT_TRUE(store.unbound());
    EXPECT_EQ(store.dim(), dim);
    EXPECT_THROW(static_cast<void>(store.extents()), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(store.volume()), std::invalid_argument);
    EXPECT_EQ(store.type(), type);
    EXPECT_FALSE(store.transformed());
    EXPECT_FALSE(store.has_scalar_storage());
  }
}

template <typename T>
void test_bound_store(const legate::Shape& shape)
{
  auto runtime = legate::Runtime::get_runtime();
  const legate::Type type{legate::primitive_type(legate::type_code_of_v<T>)};
  auto store = runtime->create_store(shape, type);

  EXPECT_FALSE(store.unbound());
  EXPECT_EQ(store.dim(), shape.ndim());
  EXPECT_EQ(store.extents(), shape.extents());
  EXPECT_EQ(store.volume(), store.extents().volume());
  EXPECT_EQ(store.type().code(), legate::type_code_of_v<T>);
  EXPECT_FALSE(store.transformed());
  EXPECT_FALSE(store.has_scalar_storage());
}

template <typename T>
void test_scalar_store(T value)
{
  auto runtime = legate::Runtime::get_runtime();
  const legate::Type type{legate::primitive_type(legate::type_code_of_v<T>)};
  auto store = runtime->create_store(legate::Scalar{value});

  EXPECT_FALSE(store.unbound());
  static constexpr std::int32_t DIM = 1;
  EXPECT_EQ(store.dim(), DIM);
  EXPECT_EQ(store.extents(), legate::tuple<std::uint64_t>{1});
  EXPECT_EQ(store.volume(), 1);
  EXPECT_EQ(store.type().code(), legate::type_code_of_v<T>);
  EXPECT_FALSE(store.transformed());
  EXPECT_TRUE(store.has_scalar_storage());
  for (const auto& extents : {legate::tuple<std::uint64_t>{1},
                              legate::tuple<std::uint64_t>{1, 1},
                              legate::tuple<std::uint64_t>{1, 1, 1}}) {
    auto temp_store = runtime->create_store(legate::Scalar{value}, extents);

    EXPECT_FALSE(temp_store.unbound());
    EXPECT_EQ(temp_store.dim(), extents.size());
    EXPECT_EQ(temp_store.extents(), extents);
    EXPECT_EQ(temp_store.type().code(), legate::type_code_of_v<T>);
    EXPECT_FALSE(temp_store.transformed());
    EXPECT_TRUE(temp_store.has_scalar_storage());
  }
}

}  // namespace

TEST_F(LogicalStoreUnit, UnboundStoreCreation)
{
  constexpr auto do_test = [](auto dim) {
    test_unbound_store<std::int8_t>(dim);
    test_unbound_store<bool>(dim);
    test_unbound_store<complex<float>>(dim);
    test_unbound_store<__half>(dim);
    test_unbound_store<std::uint64_t>(dim);
    test_unbound_store<float>(dim);
    test_unbound_store<double>(dim);
  };

  for (auto dim = 1; dim <= LEGATE_MAX_DIM; ++dim) {
    do_test(dim);
  }
}

TEST_F(LogicalStoreUnit, BoundStoreCreation)
{
  constexpr auto do_test = [](auto dim) {
    legate::tuple<std::uint64_t> extents{};

    for (auto i = 1; i <= dim; ++i) {
      extents.data().push_back(static_cast<std::uint64_t>(i));
    }
    auto shape = legate::Shape{extents};

    test_bound_store<std::uint32_t>(shape);
    test_bound_store<std::int16_t>(shape);
    test_bound_store<bool>(shape);
    test_bound_store<float>(shape);
    test_bound_store<double>(shape);
    test_bound_store<__half>(shape);
    test_bound_store<complex<double>>(shape);
  };

  for (auto dim = 1; dim <= LEGATE_MAX_DIM; ++dim) {
    do_test(dim);
  }
}

TEST_F(LogicalStoreUnit, OptimizeScalar)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store1  = runtime->create_store(legate::Shape{1}, legate::int64(), true);
  EXPECT_TRUE(store1.get_physical_store().is_future());
  EXPECT_TRUE(store1.has_scalar_storage());

  auto store2 = runtime->create_store(legate::Shape{1, 2}, legate::int64(), true);
  EXPECT_FALSE(store2.get_physical_store().is_future());
  EXPECT_FALSE(store2.has_scalar_storage());

  auto store3 = runtime->create_store(legate::Shape{1}, legate::int64(), false);
  EXPECT_FALSE(store3.get_physical_store().is_future());
  EXPECT_FALSE(store3.has_scalar_storage());
}

TEST_F(LogicalStoreUnit, EmptyShapeCreation)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{}, legate::int64());
  EXPECT_EQ(store.extents(), legate::tuple<std::uint64_t>{});
  EXPECT_EQ(store.dim(), 0);
}

TEST_F(LogicalStoreUnit, ScalarStoreCreation) { test_scalar_store<std::int64_t>(10000); }

TEST_F(LogicalStoreUnit, InvalidUnboundStoreCreation)
{
  auto runtime = legate::Runtime::get_runtime();
  // invalid dim
  EXPECT_THROW(static_cast<void>(runtime->create_store(legate::int64(), -1)), std::out_of_range);

  // create with variable size type
  EXPECT_THROW(static_cast<void>(runtime->create_store(legate::string_type(), 1)),
               std::invalid_argument);
  EXPECT_THROW(static_cast<void>(runtime->create_store(legate::list_type(legate::int32()), 2)),
               std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidScalarStoreCreation)
{
  auto runtime = legate::Runtime::get_runtime();
  // volume > 1
  EXPECT_THROW(
    static_cast<void>(runtime->create_store(legate::Scalar{std::int64_t{10}}, legate::Shape{1, 3})),
    std::invalid_argument);
}

TEST_F(LogicalStoreUnit, OverlapsUnboundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::int32(), 2);
  EXPECT_TRUE(store.overlaps(store));
  EXPECT_TRUE(store.overlaps(legate::LogicalStore{store}));

  auto other = runtime->create_store(legate::int64(), 1);
  EXPECT_FALSE(store.overlaps(other));
}

TEST_F(LogicalStoreUnit, OverlapsBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{3}, legate::int32());
  // overlap self
  EXPECT_TRUE(store.overlaps(store));

  auto optimized = runtime->create_store(legate::Shape{3}, legate::int32(), true);
  EXPECT_FALSE(store.overlaps(optimized));

  // Same root
  auto store_multi_dims = runtime->create_store(legate::Shape{4, 6, 8}, legate::int32());
  auto sliced           = store_multi_dims.slice(1, legate::Slice{1, 2});
  EXPECT_TRUE(store_multi_dims.overlaps(sliced));
  EXPECT_TRUE(sliced.overlaps(store_multi_dims));

  // Different root
  EXPECT_FALSE(store_multi_dims.overlaps(store));
  EXPECT_FALSE(store_multi_dims.overlaps(optimized));
  EXPECT_FALSE(sliced.overlaps(store));
  EXPECT_FALSE(sliced.overlaps(optimized));

  // Empty extents
  auto empty_store = runtime->create_store(legate::Shape{}, legate::int32());
  EXPECT_FALSE(empty_store.overlaps(store));
  EXPECT_FALSE(store.overlaps(empty_store));
}

TEST_F(LogicalStoreUnit, OverlapsScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{1}});
  auto other   = runtime->create_store(legate::Scalar{std::int64_t{2}});
  EXPECT_TRUE(store.overlaps(store));
  EXPECT_TRUE(store.overlaps(legate::LogicalStore{store}));
  EXPECT_FALSE(store.overlaps(legate::LogicalStore{other}));
  EXPECT_FALSE(store.overlaps(other));
}

TEST_F(LogicalStoreUnit, PromoteBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3, 2, 1}, legate::int64());

  auto test_promote = [&store](auto&& promote, auto&& shape) {
    EXPECT_EQ(promote.extents().data(), shape);
    EXPECT_TRUE(promote.transformed());
    EXPECT_EQ(promote.type(), store.type());
    EXPECT_TRUE(promote.overlaps(store));
    EXPECT_EQ(promote.dim(), store.dim() + 1);
  };

  test_promote(store.promote(0, 5), std::vector<std::uint64_t>{5, 4, 3, 2, 1});
  test_promote(store.promote(2, -1),
               std::vector<std::uint64_t>{4, 3, static_cast<std::uint64_t>(-1), 2, 1});
}

TEST_F(LogicalStoreUnit, PromoteScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  auto test_promote = [&store](auto&& promote, auto&& shape) {
    EXPECT_EQ(promote.extents().data(), shape);
    EXPECT_TRUE(promote.transformed());
    EXPECT_EQ(promote.type(), store.type());
    EXPECT_TRUE(promote.overlaps(store));
    EXPECT_EQ(promote.dim(), store.dim() + 1);
  };

  test_promote(store.promote(0, 5), std::vector<std::uint64_t>{5, 1});
  test_promote(store.promote(1, -1), std::vector<std::uint64_t>{1, static_cast<std::uint64_t>(-1)});
}

TEST_F(LogicalStoreUnit, InvalidPromoteBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  // invalid extra_dim
  EXPECT_THROW(static_cast<void>(store.promote(3, 5)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.promote(-3, 5)), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidPromoteScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  // invalid extra_dim
  EXPECT_THROW(static_cast<void>(store.promote(2, 2)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.promote(-2, 2)), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, ProjectBoundStore)
{
  auto runtime      = legate::Runtime::get_runtime();
  auto store        = runtime->create_store(legate::Shape{4, 3}, legate::int64());
  auto test_project = [&store](auto&& project, auto&& shape) {
    EXPECT_EQ(project.extents().data(), shape);
    EXPECT_TRUE(project.transformed());
    EXPECT_EQ(project.type(), store.type());
    EXPECT_TRUE(project.overlaps(store));
    EXPECT_EQ(project.dim(), 1);
  };
  test_project(store.project(0, 1), std::vector<std::uint64_t>{3});
  // project bound store where volume is 0
  store = runtime->create_store(legate::Shape{0, 4}, legate::uint32());
  test_project(store.project(1, 0), std::vector<std::uint64_t>{0});
}

TEST_F(LogicalStoreUnit, ProjectScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  auto test_project = [&store](auto&& project, auto&& shape) {
    EXPECT_EQ(project.extents().data(), shape);
    EXPECT_TRUE(project.transformed());
    EXPECT_EQ(project.type(), store.type());
    EXPECT_TRUE(project.overlaps(store));
    EXPECT_EQ(project.dim(), 0);
  };

  test_project(store.project(0, 0), std::vector<std::uint64_t>{});
}

TEST_F(LogicalStoreUnit, InvalidProjectBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.project(2, 1)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.project(-3, 1)), std::invalid_argument);
  // invalid index
  EXPECT_THROW(static_cast<void>(store.project(0, 4)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.project(0, -4)), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidProjectScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.project(1, 0)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.project(-1, 0)), std::invalid_argument);
  // invalid index
  EXPECT_THROW(static_cast<void>(store.project(0, 1)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.project(0, -1)), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, SliceBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  auto test_slice = [&store](auto&& slice, auto&& shape, bool transformed, bool overlaps) {
    EXPECT_EQ(slice.extents().data(), shape);
    EXPECT_EQ(slice.transformed(), transformed);
    EXPECT_EQ(slice.type(), store.type());
    EXPECT_EQ(slice.overlaps(store), overlaps);
    EXPECT_EQ(slice.dim(), store.dim());
  };

  test_slice(store.slice(1, legate::Slice{-2, -1}), std::vector<std::uint64_t>{4, 1}, true, true);
  test_slice(store.slice(1, legate::Slice{1, 2}), std::vector<std::uint64_t>{4, 1}, true, true);
  test_slice(store.slice(0, legate::Slice{}), std::vector<std::uint64_t>{4, 3}, false, true);
  test_slice(store.slice(0, legate::Slice{0, 0}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(store.slice(1, legate::Slice{1, 1}), std::vector<std::uint64_t>{4, 0}, false, false);

  test_slice(store.slice(0, legate::Slice{-9, -8}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(
    store.slice(0, legate::Slice{-8, -10}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(store.slice(0, legate::Slice{1, 1}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(store.slice(0, legate::Slice{-1, 0}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(store.slice(0, legate::Slice{-1, 1}), std::vector<std::uint64_t>{0, 3}, false, false);
  test_slice(store.slice(0, legate::Slice{10, 8}), std::vector<std::uint64_t>{0, 3}, false, false);
}

TEST_F(LogicalStoreUnit, SliceScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  auto test_slice = [&store](auto&& slice, auto&& shape, bool transformed, bool overlaps) {
    EXPECT_EQ(slice.extents().data(), shape);
    EXPECT_EQ(slice.transformed(), transformed);
    EXPECT_EQ(slice.type(), store.type());
    EXPECT_EQ(slice.overlaps(store), overlaps);
    EXPECT_EQ(slice.dim(), store.dim());
  };

  test_slice(store.slice(0, legate::Slice{-2, -1}), std::vector<std::uint64_t>{0}, false, false);
  test_slice(store.slice(0, legate::Slice{}), std::vector<std::uint64_t>{1}, false, true);
  test_slice(store.slice(0, legate::Slice{0, 0}), std::vector<std::uint64_t>{0}, false, false);
  test_slice(store.slice(0, legate::Slice{1, 1}), std::vector<std::uint64_t>{0}, false, false);
}

TEST_F(LogicalStoreUnit, InvalidSliceBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.slice(2, legate::Slice{1, 3})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.slice(-2, legate::Slice{1, 3})), std::invalid_argument);

  // Out of bounds
  EXPECT_THROW(static_cast<void>(store.slice(0, legate::Slice{4, 5})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.slice(1, legate::Slice{1, 4})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidSliceScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.slice(1, legate::Slice{0, 1})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.slice(-1, legate::Slice{0, 1})), std::invalid_argument);

  // Out of bounds
  EXPECT_THROW(static_cast<void>(store.slice(0, legate::Slice{0, 2})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, TransposeBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  auto test_transpose = [&store](auto&& transpose, auto&& shape) {
    EXPECT_EQ(transpose.extents().data(), shape);
    EXPECT_TRUE(transpose.transformed());
    EXPECT_EQ(transpose.type(), store.type());
    EXPECT_TRUE(transpose.overlaps(store));
    EXPECT_EQ(transpose.dim(), store.dim());
  };

  test_transpose(store.transpose({1, 0}), std::vector<std::uint64_t>{3, 4});
}

TEST_F(LogicalStoreUnit, TransposeScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  auto test_transpose = [&store](auto&& transpose, auto&& shape) {
    EXPECT_EQ(transpose.extents().data(), shape);
    EXPECT_TRUE(transpose.transformed());
    EXPECT_EQ(transpose.type(), store.type());
    EXPECT_TRUE(transpose.overlaps(store));
    EXPECT_EQ(transpose.dim(), store.dim());
  };

  test_transpose(store.transpose({0}), std::vector<std::uint64_t>{1});
}

TEST_F(LogicalStoreUnit, InvalidTransposeBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  // invalid axes length
  EXPECT_THROW(static_cast<void>(store.transpose({2})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.transpose({2, 1, 0})), std::invalid_argument);
  // axes has duplicates
  EXPECT_THROW(static_cast<void>(store.transpose({0, 0})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.transpose({-1, -1})), std::invalid_argument);
  // invalid axis in axes
  EXPECT_THROW(static_cast<void>(store.transpose({2, 0})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.transpose({-2, 0})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidTransposeScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  // invalid axes length, axes has duplicates
  EXPECT_THROW(static_cast<void>(store.transpose({0, 0})), std::invalid_argument);
  // invalid axis in axes
  EXPECT_THROW(static_cast<void>(store.transpose({2})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.transpose({-2})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, DelinearizeBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  auto test_delinearize = [&store](auto&& delinearize, auto&& shape) {
    EXPECT_EQ(delinearize.extents().data(), shape);
    EXPECT_TRUE(delinearize.transformed());
    EXPECT_EQ(delinearize.type(), store.type());
    EXPECT_TRUE(delinearize.overlaps(store));
    EXPECT_EQ(delinearize.dim(), shape.size());
  };

  test_delinearize(store.delinearize(0, std::vector<std::uint64_t>{2, 2}),
                   std::vector<std::uint64_t>{2, 2, 3});
  test_delinearize(store.delinearize(1, std::vector<std::uint64_t>{1, 3, 1}),
                   std::vector<std::uint64_t>{4, 1, 3, 1});
}

TEST_F(LogicalStoreUnit, DelinearizeScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::int64_t{10}});

  auto test_delinearize = [&store](auto&& delinearize, auto&& shape) {
    EXPECT_EQ(delinearize.extents().data(), shape);
    EXPECT_TRUE(delinearize.transformed());
    EXPECT_EQ(delinearize.type(), store.type());
    EXPECT_TRUE(delinearize.overlaps(store));
    EXPECT_EQ(delinearize.dim(), store.dim() + shape.size() - 1);
  };

  test_delinearize(store.delinearize(0, std::vector<std::uint64_t>{1, 1, 1}),
                   std::vector<std::uint64_t>{1, 1, 1});
}

TEST_F(LogicalStoreUnit, InvalidDelinearizeBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 3}, legate::int64());

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.delinearize(2, {2, 3})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.delinearize(-2, {2, 3})), std::invalid_argument);
  // invalid sizes
  EXPECT_THROW(static_cast<void>(store.delinearize(0, {2, 3})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidDelinearizeScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::uint64_t{10}});

  // invalid dim
  EXPECT_THROW(static_cast<void>(store.delinearize(1, {1, 1})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.delinearize(-1, {1, 1})), std::invalid_argument);
  // invalid sizes
  EXPECT_THROW(static_cast<void>(store.delinearize(0, {2, 3})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, TransformUnboundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::int64());
  EXPECT_THROW(static_cast<void>(store.promote(1, 1)), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.slice(0, legate::Slice{1})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.transpose({-1, -1})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.delinearize(0, {1})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, PartitionBoundStore)
{
  auto runtime   = legate::Runtime::get_runtime();
  auto store     = runtime->create_store(legate::Shape{7, 8}, legate::int64());
  auto partition = store.partition_by_tiling({2, 4});
  const std::vector<std::uint64_t> shape1{4, 2};

  EXPECT_EQ(partition.color_shape().data(), shape1);

  partition = store.partition_by_tiling({5, 3});
  const std::vector<std::uint64_t> shape2{2, 3};

  EXPECT_EQ(partition.color_shape().data(), shape2);

  partition = store.partition_by_tiling({1, 1});
  const std::vector<std::uint64_t> shape3{7, 8};
  EXPECT_EQ(partition.color_shape().data(), shape3);

  partition = store.partition_by_tiling({7, 8});
  const std::vector<std::uint64_t> shape4{1, 1};
  EXPECT_EQ(partition.color_shape().data(), shape4);

  partition = store.partition_by_tiling({8, 4});
  const std::vector<std::uint64_t> shape5{1, 2};
  EXPECT_EQ(partition.color_shape().data(), shape5);

  partition = store.partition_by_tiling({9, 5});
  const std::vector<std::uint64_t> shape6{1, 2};
  EXPECT_EQ(partition.color_shape().data(), shape6);

  partition = store.partition_by_tiling({2, 10});
  const std::vector<std::uint64_t> shape7{4, 1};
  EXPECT_EQ(partition.color_shape().data(), shape7);

  partition = store.partition_by_tiling({10, 20});
  const std::vector<std::uint64_t> shape8{1, 1};
  EXPECT_EQ(partition.color_shape().data(), shape8);
}

TEST_F(LogicalStoreUnit, PartitionScalarStore)
{
  auto runtime   = legate::Runtime::get_runtime();
  auto store     = runtime->create_store(legate::Scalar{std::uint64_t{10}});
  auto partition = store.partition_by_tiling({1});
  EXPECT_EQ(partition.color_shape().data(), std::vector<std::uint64_t>{1});
}

TEST_F(LogicalStoreUnit, InvalidPartitionBoundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Shape{4, 4}, legate::int64());
  // shape size mismatch
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({1})), std::invalid_argument);

  // volume is 0
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({0, 0})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidPartitionScalarStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::Scalar{std::uint32_t{1}});
  // shape size mismatch
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({1, 1})), std::invalid_argument);

  // volume is 0
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({0, 0})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, InvalidPartitionUnboundStore)
{
  auto runtime = legate::Runtime::get_runtime();
  auto store   = runtime->create_store(legate::int64());
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({})), std::invalid_argument);
  EXPECT_THROW(static_cast<void>(store.partition_by_tiling({0})), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, ChildStore)
{
  auto runtime   = legate::Runtime::get_runtime();
  auto store     = runtime->create_store(legate::Shape{9, 8}, legate::int64());
  auto partition = store.partition_by_tiling({2, 4});
  const std::vector<std::uint64_t> shape1{5, 2};
  EXPECT_EQ(partition.color_shape().data(), shape1);
  EXPECT_EQ(partition.store().shape(), store.shape());
  EXPECT_EQ(partition.get_child_store({0, 0}).shape(), (legate::Shape{2, 4}));
  EXPECT_EQ(partition.get_child_store({2, 1}).shape(), (legate::Shape{2, 4}));
  EXPECT_EQ(partition.get_child_store({4, 1}).shape(), (legate::Shape{1, 4}));

  // TODO(issue 686)
  // EXPECT_EQ(partition.get_child_store({5, 1}).shape(), (legate::Shape{18446744073709551615, 4}));
  // EXPECT_EQ(partition.get_child_store({0, 2}).shape(), (legate::Shape{2, 0}));

  partition = store.partition_by_tiling({3, 5});
  const std::vector<std::uint64_t> shape2{3, 2};
  EXPECT_EQ(partition.color_shape().data(), shape2);
  EXPECT_EQ(partition.store().shape(), store.shape());
  EXPECT_EQ(partition.get_child_store({0, 0}).shape(), (legate::Shape{3, 5}));
  EXPECT_EQ(partition.get_child_store({1, 1}).shape(), (legate::Shape{3, 3}));
  EXPECT_EQ(partition.get_child_store({2, 1}).shape(), (legate::Shape{3, 3}));

  // TODO(issue 686)
  // EXPECT_EQ(partition.get_child_store({3, 1}).shape(), (legate::Shape{0, 3}));
  // EXPECT_EQ(partition.get_child_store({0, 2}).shape(), (legate::Shape{3, 18446744073709551615}));
}

TEST_F(LogicalStoreUnit, InvalidChildStore)
{
  auto runtime   = legate::Runtime::get_runtime();
  auto store     = runtime->create_store(legate::Shape{4, 8}, legate::int64());
  auto partition = store.partition_by_tiling({1, 4});
  const std::vector<std::uint64_t> shape{4, 2};
  EXPECT_EQ(partition.color_shape().data(), shape);

  // Invalid color
  EXPECT_THROW(static_cast<void>(partition.get_child_store({4, 2})), std::out_of_range);
  EXPECT_THROW(static_cast<void>(partition.get_child_store({5, 2})), std::out_of_range);
  EXPECT_THROW(static_cast<void>(partition.get_child_store({5, 0})), std::out_of_range);
}

TEST_F(LogicalStoreUnit, PhysicalStoreBound)
{
  auto runtime        = legate::Runtime::get_runtime();
  auto store          = runtime->create_store(legate::Shape{4, 4}, legate::int64());
  auto physical_store = store.get_physical_store();
  EXPECT_FALSE(physical_store.is_unbound_store());
  EXPECT_FALSE(physical_store.is_future());
  constexpr std::int32_t DIM = 2;
  EXPECT_EQ(physical_store.dim(), DIM);
  EXPECT_EQ(physical_store.shape<DIM>(), (legate::Rect<DIM>({0, 0}, {3, 3})));
  EXPECT_TRUE(physical_store.valid());
  EXPECT_EQ(physical_store.code(), legate::Type::Code::INT64);
}

TEST_F(LogicalStoreUnit, PhysicalStoreBoundEmptyShape)
{
  auto runtime        = legate::Runtime::get_runtime();
  auto store          = runtime->create_store(legate::Shape{}, legate::int64());
  auto physical_store = store.get_physical_store();
  EXPECT_FALSE(physical_store.is_unbound_store());
  EXPECT_FALSE(physical_store.is_future());
  EXPECT_EQ(physical_store.dim(), 0);
  // shape of 0-D store
  constexpr std::int32_t DIM = 1;
  EXPECT_EQ(physical_store.shape<DIM>(), legate::Rect<DIM>(0, 0));
  EXPECT_TRUE(physical_store.valid());
  EXPECT_EQ(physical_store.code(), legate::Type::Code::INT64);
}

TEST_F(LogicalStoreUnit, PhysicalStoreUnbound)
{
  auto runtime               = legate::Runtime::get_runtime();
  constexpr std::int32_t DIM = 3;
  auto store                 = runtime->create_store(legate::int64(), DIM);
  EXPECT_THROW(static_cast<void>(store.get_physical_store()), std::invalid_argument);
}

TEST_F(LogicalStoreUnit, PhysicalStoreScalar)
{
  auto runtime        = legate::Runtime::get_runtime();
  auto store          = runtime->create_store(legate::Scalar{std::int32_t{10}});
  auto physical_store = store.get_physical_store();
  EXPECT_FALSE(physical_store.is_unbound_store());
  EXPECT_TRUE(physical_store.is_future());
  static constexpr std::int32_t DIM = 1;
  EXPECT_EQ(physical_store.dim(), DIM);
  EXPECT_EQ(physical_store.shape<DIM>(), (legate::Rect<DIM>(0, 0)));
  EXPECT_TRUE(physical_store.valid());
  EXPECT_EQ(physical_store.code(), legate::Type::Code::INT32);
}

TEST_F(LogicalStoreUnit, EqualStorageSelf)
{
  const auto runtime = legate::Runtime::get_runtime();
  const auto store   = runtime->create_store(legate::Scalar{std::int32_t{10}});

  EXPECT_TRUE(store.equal_storage(store));
}

TEST_F(LogicalStoreUnit, EqualStorageNotEqual)
{
  const auto runtime = legate::Runtime::get_runtime();
  const auto store   = runtime->create_store(legate::Scalar{std::int32_t{10}});
  const auto store2  = runtime->create_store(legate::Scalar{std::int32_t{10}});

  // Unrelated stores are in fact, unrelated
  EXPECT_FALSE(store.equal_storage(store2));
  EXPECT_FALSE(store2.equal_storage(store));
}

TEST_F(LogicalStoreUnit, EqualStorageSliced)
{
  const auto runtime = legate::Runtime::get_runtime();
  /// [Store::equal_storage: Comparing sliced stores]
  const auto store       = runtime->create_store(legate::Shape{4, 3}, legate::int64());
  const auto transformed = store.slice(1, legate::Slice{-2, -1});

  // Slices partition a store into a parent and sub-store which both cover distinct regions,
  // and hence don't share storage.
  EXPECT_FALSE(store.equal_storage(transformed));
  /// [Store::equal_storage: Comparing sliced stores]
  EXPECT_FALSE(transformed.equal_storage(store));
}

TEST_F(LogicalStoreUnit, EqualStorageTranspoe)
{
  const auto runtime = legate::Runtime::get_runtime();
  /// [Store::equal_storage: Comparing transposed stores]
  const auto store       = runtime->create_store(legate::Shape{4, 3}, legate::int64());
  const auto transformed = store.transpose({1, 0});

  // Transposing a store doesn't modify the storage
  EXPECT_TRUE(store.equal_storage(transformed));
  /// [Store::equal_storage: Comparing transposed stores]
  EXPECT_TRUE(transformed.equal_storage(store));

  const auto transformed2 = transformed.transpose({1, 0});

  EXPECT_TRUE(transformed.equal_storage(transformed2));
  EXPECT_TRUE(transformed2.equal_storage(transformed));
  EXPECT_TRUE(store.equal_storage(transformed2));
  EXPECT_TRUE(transformed2.equal_storage(store));
}

TEST_F(LogicalStoreUnit, EqualStorageFuture)
{
  const auto runtime  = legate::Runtime::get_runtime();
  const auto store    = runtime->create_store(legate::Scalar{std::int32_t{10}});
  const auto promoted = store.promote(0, 5);
  EXPECT_TRUE(promoted.get_physical_store().is_future());
  EXPECT_TRUE(promoted.equal_storage(store));
  EXPECT_TRUE(store.equal_storage(promoted));
}

TEST_F(LogicalStoreUnit, ToString)
{
  const auto runtime     = legate::Runtime::get_runtime();
  const auto bound_store = runtime->create_store(legate::Scalar{std::int32_t{10}});

  ASSERT_THAT(
    bound_store.to_string(),
    ::testing::MatchesRegex(
      R"(Store\([0-9]+\) \{shape: \[1\], storage: Storage\([0-9]+\) \{kind: Future, type: \w+, level: [0-9]+\}\})"));

  const auto promoted = bound_store.promote(0, 5);

  ASSERT_THAT(
    promoted.to_string(),
    ::testing::MatchesRegex(
      R"(Store\([0-9]+\) \{shape: \[5, 1\], transform: Promote\(extra_dim: 0, dim_size: 5\), storage: Storage\([0-9]+\) \{kind: Future, type: \w+, level: [0-9]+\}\})"));

  const auto unbound_store = runtime->create_store(legate::int64());

  ASSERT_THAT(
    unbound_store.to_string(),
    ::testing::MatchesRegex(
      R"(Store\([0-9]+\) \{shape: \(unbound\), storage: Storage\([0-9]+\) \{kind: \w+, type: \w+, level: [0-9]+, region: unbound\}\})"));
}

// NOLINTEND(readability-magic-numbers)

}  // namespace logicalstore_unit
