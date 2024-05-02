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

#include "stlfwd.hpp"
/////////////////
#include "get_logical_store.hpp"
#include "legate.h"
#include "mdspan.hpp"
#include "slice.hpp"
#include "span.hpp"

#include <array>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>

// Include this last:
#include "prefix.hpp"

namespace legate::experimental::stl {

/** @cond */
template <typename ElementType, std::int32_t Dim>
class logical_store;

namespace detail {

template <typename ElementType, std::int32_t Dim>
class value_type_of_<logical_store<ElementType, Dim>> {
 public:
  using type = ElementType;
};

struct ctor_tag {};

}  // namespace detail
/** @endcond */

/**
 * @brief A multi-dimensional data container
 *
 * `logical_store` is a multi-dimensional data container for fixed-size elements. Stores are
 * internally partitioned and distributed across the system, but users need not specify the
 * partitioning and distribution directly. Instead, Legate.STL automatically partitions and
 * distributes stores based on how the `logical_store` object is used.
 *
 * `logical_store` objects are value-like and are move-only. They are not immediately associated
 * with a physical allocation. To access the data directly, users must ask for a view into
 * the logical store. This will trigger the allocation of a physical store and the population
 * of the physical memory, which is a blocking operation.
 *
 * @tparam ElementType The type of the elements of the store. The element type must be fixed
 *         size, and it must be one of the
 *         @verbatim embed:rst:inline :ref:`allowable Legate element types <element-types>`.
           @endverbatim
 * @tparam Dim The number of dimensions in the logical store
 *
 * @verbatim embed:rst:leading-asterisk
 * @endverbatim
 */
template <typename ElementType, std::int32_t Dim>
class logical_store : private legate::LogicalStore {
 public:
  static_assert(
    type_code_of<ElementType> != legate::Type::Code::NIL,
    "The type of a logical_store<> must be a type that is valid for legate::LogicalStore.");
  using value_type = ElementType;
  // By default, the algorithms treat stores as element-wise.
  using policy = detail::element_policy::policy<ElementType, Dim>;

  logical_store() = delete;

  /**
   * @brief Create a logical store of a given shape.
   *
   * @tparam Rank The number of dimensions in the store.
   * @param extents The extents of the store.
   *
   * @code
   * logical_store<int, 3> data{{ 10, 20, 30 }};
   * @endcode
   */
  template <std::size_t Rank>
    requires(Rank == Dim)
  explicit logical_store(const std::size_t (&extents)[Rank])
    : LogicalStore{logical_store::create(extents)}
  {
  }

  /**
   * @overload
   */
  explicit logical_store(std::span<const std::size_t, Dim> extents)
    : LogicalStore{logical_store::create(std::move(extents))}
  {
  }

  /**
   * @brief Create a logical store of a given shape and fills it with a
   *        given value.
   *
   * @tparam Rank The number of dimensions in the store.
   * @param extents The extents of the store.
   * @param value The value to fill the store with.
   *
   * @code
   * logical_store<int, 3> data{{ 10, 20, 30 }, 42};
   * @endcode
   */
  template <std::size_t Rank>
    requires(Rank == Dim)
  explicit logical_store(const std::size_t (&extents)[Rank], ElementType value)
    : LogicalStore{logical_store::create(extents)}
  {
    legate::Runtime::get_runtime()->issue_fill(*this, Scalar{std::move(value)});
  }

  /**
   * @overload
   */
  explicit logical_store(std::span<const std::size_t, Dim> extents, ElementType value)
    : LogicalStore{logical_store::create(std::move(extents))}
  {
    legate::Runtime::get_runtime()->issue_fill(*this, Scalar{std::move(value)});
  }

  /**
   * @brief `logical_store` is a move-only type.
   */
  logical_store(logical_store&&)            = default;
  logical_store& operator=(logical_store&&) = default;

  /**
   * @brief Get the dimensionality of the store.
   *
   * @return `std::int32_t` - The number of dimensions in the store.
   */
  [[nodiscard]] static constexpr std::int32_t dim() noexcept { return Dim; }

  /**
   * @brief Retrieve the extents of the store as a `std::array`
   *
   * @return `std::array<std::size_t, Dim>` - The extents of the store.
   */
  [[nodiscard]] std::array<std::size_t, Dim> extents() const noexcept
  {
    auto&& extents = LogicalStore::extents();
    std::array<std::size_t, Dim> result;

    LegateAssert(extents.size() == Dim);
    std::copy(&extents[0], &extents[0] + Dim, result.begin());
    return result;
  }

 private:
  template <typename, std::int32_t>
  friend class logical_store;

  [[nodiscard]] static LogicalStore create(std::span<const std::size_t, Dim> exts)
  {
    Runtime* runtime = legate::Runtime::get_runtime();
    // create_store() takes const-ref for now, but may not always be the case
    // NOLINTNEXTLINE(misc-const-correctness)
    Shape shape({exts.begin(), exts.end()});
    return runtime->create_store(std::move(shape), primitive_type(type_code_of<ElementType>));
  }

  static void validate(const LogicalStore& store)
  {
    static_assert(sizeof(logical_store) == sizeof(LogicalStore));
    LegateAssert(store.type().code() == type_code_of<ElementType>);
    LegateAssert(store.dim() == Dim || (Dim == 0 && store.dim() == 1));
  }

  logical_store(detail::ctor_tag, LogicalStore&& store) : LogicalStore{std::move(store)}
  {
    validate(*this);
  }

  logical_store(detail::ctor_tag, const LogicalStore& store) : LogicalStore{store}
  {
    validate(*this);
  }

  friend logical_store<ElementType, Dim> as_typed<>(const LogicalStore& store);

  friend LogicalStore get_logical_store(const logical_store& self) noexcept { return self; }

  friend auto as_range(logical_store& self) noexcept { return elements_of(self); }

  friend auto as_range(const logical_store& self) noexcept { return elements_of(self); }
};

/** @cond */
// A specialization for 0-dimensional (scalar) stores:
template <typename ElementType>
class logical_store<ElementType, 0> : private LogicalStore {
 public:
  using value_type = ElementType;
  // By default, the algorithms treat stores as element-wise.
  using policy = detail::element_policy::policy<ElementType, 0>;

  logical_store() = delete;

  explicit logical_store(std::span<const std::size_t, 0>) : LogicalStore{logical_store::create()} {}

  explicit logical_store(std::span<const std::size_t, 0>, ElementType elem)
    : LogicalStore{logical_store::create(std::move(elem))}
  {
  }

  // Make logical_store a move-only type:
  logical_store(logical_store&&)            = default;
  logical_store& operator=(logical_store&&) = default;

  [[nodiscard]] static constexpr std::int32_t dim() noexcept { return 0; }

  [[nodiscard]] std::array<std::size_t, 0> extents() const { return {}; }

 private:
  template <typename, std::int32_t>
  friend class logical_store;

  [[nodiscard]] static LogicalStore create(ElementType elem = {})
  {
    return legate::Runtime::get_runtime()->create_store(Scalar{std::move(elem)});
  }

  logical_store(detail::ctor_tag, LogicalStore&& store) : LogicalStore{std::move(store)} {}

  logical_store(detail::ctor_tag, const LogicalStore& store) : LogicalStore{store} {}

  friend logical_store<ElementType, 0> as_typed<>(const LogicalStore& store);

  friend LogicalStore get_logical_store(const logical_store& self) noexcept { return self; }

  friend auto as_range(logical_store& self) noexcept { return elements_of(self); }

  friend auto as_range(const logical_store& self) noexcept { return elements_of(self); }
};
/** @endcond */

/*-***********************************************************************************************
 * Deduction guides for logical_store<>:
 */
template <typename ElementType>
logical_store(std::span<const std::size_t, 0>, ElementType) -> logical_store<ElementType, 0>;

template <typename ElementType, std::size_t Dim>
logical_store(const std::size_t (&)[Dim], ElementType) -> logical_store<ElementType, Dim>;

template <typename ElementType, std::size_t Dim>
logical_store(std::array<std::size_t, Dim>, ElementType) -> logical_store<ElementType, Dim>;

/**
 * @brief Given an untyped `legate::LogicalStore`, return a strongly-typed
 *        `legate::experimental::stl::logical_store`.
 *
 * @tparam ElementType The element type of the `LogicalStore`.
 * @tparam Dim The dimensionality of the `LogicalStore`.
 * @param store The `LogicalStore` to convert.
 * @return `logical_store<ElementType, Dim>`
 * @pre The element type of the `LogicalStore` must be the same as `ElementType`,
 *      and the dimensionality of the `LogicalStore` must be the same as `Dim`.
 */
template <typename ElementType, std::int32_t Dim>
[[nodiscard]] logical_store<ElementType, Dim> as_typed(const legate::LogicalStore& store)
{
  return {detail::ctor_tag{}, store};
}

/** @cond */
namespace detail {

template <std::int32_t Dim, typename Rect>
[[nodiscard]] inline std::array<coord_t, Dim> dynamic_extents(const Rect& shape)
{
  if constexpr (Dim == 0) {
    return {};
  } else {
    const Point<Dim> extents = Point<Dim>::ONES() + shape.hi - shape.lo;
    std::array<coord_t, Dim> result;
    for (std::int32_t i = 0; i < Dim; ++i) {  //
      result[i] = extents[i];
    }
    return result;
  }
}

template <std::int32_t Dim>
[[nodiscard]] inline std::array<coord_t, Dim> dynamic_extents(const legate::PhysicalStore& store)
{
  return dynamic_extents<Dim>(store.shape < Dim ? Dim : 1 > ());
}
}  // namespace detail
/** @endcond */

/**
 * @brief Given an untyped `legate::PhysicalStore`, return a strongly-typed
 *        `legate::experimental::stl::logical_store`.
 *
 * @tparam ElementType The element type of the `PhysicalStore`.
 * @tparam Dim The dimensionality of the `PhysicalStore`.
 * @param store The `PhysicalStore` to convert.
 * @return `mdspan_t<ElementType, Dim>`
 * @pre The element type of the `PhysicalStore` must be the same as
 *      `ElementType`, and the dimensionality of the `PhysicalStore` must be the
 *      same as `Dim`.
 */
template <typename ElementType, std::int32_t Dim>
LEGATE_HOST_DEVICE [[nodiscard]] inline mdspan_t<ElementType, Dim> as_mdspan(
  const legate::PhysicalStore& store)
{
  // These can all be *sometimes* moved.
  // NOLINTBEGIN(misc-const-correctness)
  using Mapping = std::layout_right::mapping<std::dextents<coord_t, Dim>>;
  Mapping mapping{detail::dynamic_extents<Dim>(store)};

  using Accessor = detail::mdspan_accessor<ElementType, Dim>;
  Accessor accessor{store};

  using Handle = typename Accessor::data_handle_type;
  Handle handle{};
  // NOLINTEND(misc-const-correctness)

  return {std::move(handle), std::move(mapping), std::move(accessor)};
}

/**
 * @overload
 */
template <typename ElementType, std::int32_t Dim>
LEGATE_HOST_DEVICE [[nodiscard]] inline mdspan_t<ElementType, Dim> as_mdspan(
  const legate::LogicalStore& store)
{
  return stl::as_mdspan<ElementType, Dim>(store.get_physical_store());
}

/**
 * @overload
 */
template <typename ElementType, std::int32_t Dim>
LEGATE_HOST_DEVICE [[nodiscard]] inline mdspan_t<ElementType, Dim> as_mdspan(
  const legate::PhysicalArray& array)
{
  return stl::as_mdspan<ElementType, Dim>(array.data());
}

/**
 * @overload
 */
template <typename ElementType, std::int32_t Dim, template <typename, std::int32_t> typename StoreT>
  requires(same_as<logical_store<ElementType, Dim>, StoreT<ElementType, Dim>>)
LEGATE_HOST_DEVICE [[nodiscard]] inline mdspan_t<ElementType, Dim> as_mdspan(
  const StoreT<ElementType, Dim>& store)
{
  return stl::as_mdspan<ElementType, Dim>(get_logical_store(store));
}

/**
 * @overload
 */
template <typename ElementType, typename Extents, typename Layout, typename Accessor>
LEGATE_HOST_DEVICE [[nodiscard]] inline auto as_mdspan(
  std::mdspan<ElementType, Extents, Layout, Accessor> mdspan)
  -> std::mdspan<ElementType, Extents, Layout, Accessor>
{
  return mdspan;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** @cond */
namespace detail {

template <bool>
class as_mdspan_result {
 public:
  template <typename T, typename ElementType, typename Dim>
  using eval = decltype(stl::as_mdspan<ElementType, Dim::value>(std::declval<T>()));
};

template <>
class as_mdspan_result<true> {
 public:
  template <typename T, typename ElementType, typename Dim>
  using eval = decltype(stl::as_mdspan(std::declval<T>()));
};

}  // namespace detail
/** @endcond */

template <typename T, typename ElementType = void, std::int32_t Dim = -1>
using as_mdspan_t = meta::eval<detail::as_mdspan_result<same_as<ElementType, void> && Dim == -1>,
                               T,
                               ElementType,
                               meta::constant<Dim>>;

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ElementType>  //
[[nodiscard]] logical_store<ElementType, 0> create_store(std::span<const std::size_t, 0>)
{
  return logical_store<ElementType, 0>{{}};
}

template <typename ElementType>  //
[[nodiscard]] logical_store<ElementType, 0> create_store(std::span<const std::size_t, 0>,
                                                         ElementType value)
{
  return logical_store<ElementType, 0>{{}, std::move(value)};
}

template <typename ElementType, std::int32_t Dim>  //
[[nodiscard]] logical_store<ElementType, Dim> create_store(const std::size_t (&exts)[Dim])
{
  return logical_store<ElementType, Dim>{exts};
}

template <typename ElementType, std::int32_t Dim>  //
[[nodiscard]] logical_store<ElementType, Dim> create_store(std::span<const std::size_t, Dim> exts)
{
  return logical_store<ElementType, Dim>{exts};
}

template <typename ElementType, std::int32_t Dim>  //
[[nodiscard]] logical_store<ElementType, Dim> create_store(const std::size_t (&exts)[Dim],
                                                           ElementType value)
{
  return logical_store<ElementType, Dim>{exts, std::move(value)};
}

template <typename ElementType, std::int32_t Dim>  //
[[nodiscard]] logical_store<ElementType, Dim> create_store(std::span<const std::size_t, Dim> exts,
                                                           ElementType value)
{
  return logical_store<ElementType, Dim>{exts, std::move(value)};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ElementType>
[[nodiscard]] logical_store<ElementType, 0> scalar(ElementType value)
{
  return logical_store<ElementType, 0>{{}, std::move(value)};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename SlicePolicy, typename ElementType, std::int32_t Dim>
[[nodiscard]] auto slice_as(const logical_store<ElementType, Dim>& store)
{
  return slice_view<ElementType, Dim, SlicePolicy>{get_logical_store(store)};
}

}  // namespace legate::experimental::stl

#include "suffix.hpp"
