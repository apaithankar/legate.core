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

#include "core/data/buffer.h"
#include "core/data/inline_allocation.h"
#include "core/mapping/mapping.h"
#include "core/type/type_traits.h"
#include "core/utilities/compiler.h"
#include "core/utilities/dispatch.h"
#include "core/utilities/internal_shared_ptr.h"
#include "core/utilities/shared_ptr.h"

/** @defgroup data Data abstractions and allocators
 */

/**
 * @file
 * @brief Class definition for legate::PhysicalStore
 */

namespace legate::detail {
class PhysicalStore;
}  // namespace legate::detail

namespace legate {

class PhysicalArray;

#define LEGATE_CORE_TRUE_WHEN_DEBUG LEGATE_DEFINED(LEGATE_USE_DEBUG)

/**
 * @ingroup data
 *
 * @brief A multi-dimensional data container storing task data
 */
class PhysicalStore {
 public:
  /**
   * @brief Returns a read-only accessor to the store for the entire domain.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @return A read-only accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRO<T, DIM> read_accessor() const;
  /**
   * @brief Returns a write-only accessor to the store for the entire domain.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @return A write-only accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorWO<T, DIM> write_accessor() const;
  /**
   * @brief Returns a read-write accessor to the store for the entire domain.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @return A read-write accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRW<T, DIM> read_write_accessor() const;
  /**
   * @brief Returns a reduction accessor to the store for the entire domain.
   *
   * @tparam OP Reduction operator class. For details about reduction operators, See
   * Library::register_reduction_operator.
   *
   * @tparam EXCLUSIVE Indicates whether reductions can be performed in exclusive mode. If
   * `EXCLUSIVE` is `false`, every reduction via the accessor is performed atomically.
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @return A reduction accessor to the store
   */
  template <typename OP,
            bool EXCLUSIVE,
            std::int32_t DIM,
            bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRD<OP, EXCLUSIVE, DIM> reduce_accessor() const;

  /**
   * @brief Returns a read-only accessor to the store for specific bounds.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @param bounds Domain within which accesses should be allowed.
   * The actual bounds for valid access are determined by an intersection between
   * the store's domain and the bounds.
   *
   * @return A read-only accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRO<T, DIM> read_accessor(const Rect<DIM>& bounds) const;
  /**
   * @brief Returns a write-only accessor to the store for the entire domain.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @param bounds Domain within which accesses should be allowed.
   * The actual bounds for valid access are determined by an intersection between
   * the store's domain and the bounds.
   *
   * @return A write-only accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorWO<T, DIM> write_accessor(const Rect<DIM>& bounds) const;
  /**
   * @brief Returns a read-write accessor to the store for the entire domain.
   *
   * @tparam T Element type
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @param bounds Domain within which accesses should be allowed.
   * The actual bounds for valid access are determined by an intersection between
   * the store's domain and the bounds.
   *
   * @return A read-write accessor to the store
   */
  template <typename T, std::int32_t DIM, bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRW<T, DIM> read_write_accessor(const Rect<DIM>& bounds) const;
  /**
   * @brief Returns a reduction accessor to the store for the entire domain.
   *
   * @param bounds Domain within which accesses should be allowed.
   * The actual bounds for valid access are determined by an intersection between
   * the store's domain and the bounds.
   *
   * @tparam OP Reduction operator class. For details about reduction operators, See
   * Library::register_reduction_operator.
   *
   * @tparam EXCLUSIVE Indicates whether reductions can be performed in exclusive mode. If
   * `EXCLUSIVE` is `false`, every reduction via the accessor is performed atomically.
   *
   * @tparam DIM Number of dimensions
   *
   * @tparam VALIDATE_TYPE If `true` (default), validates type and number of dimensions
   *
   * @return A reduction accessor to the store
   */
  template <typename OP,
            bool EXCLUSIVE,
            std::int32_t DIM,
            bool VALIDATE_TYPE = LEGATE_CORE_TRUE_WHEN_DEBUG>
  [[nodiscard]] AccessorRD<OP, EXCLUSIVE, DIM> reduce_accessor(const Rect<DIM>& bounds) const;

  /**
   * @brief Returns the scalar value stored in the store.
   *
   * The requested type must match with the store's data type. If the store is not
   * backed by the future, the runtime will fail with an error message.
   *
   * @tparam VAL Type of the scalar value
   *
   * @return The scalar value stored in the store
   */
  template <typename VAL>
  [[nodiscard]] VAL scalar() const;

  /**
   * @brief Creates a buffer of specified extents for the unbound store.
   *
   * The returned buffer is always consistent with the mapping policy for the store. Can be invoked
   * multiple times unless `bind_buffer` is true.
   *
   * @param extents Extents of the buffer
   *
   * @param bind_buffer If the value is true, the created buffer will be bound
   * to the store upon return
   *
   * @return A reduction accessor to the store
   */
  template <typename T, std::int32_t DIM>
  [[nodiscard]] Buffer<T, DIM> create_output_buffer(const Point<DIM>& extents,
                                                    bool bind_buffer = false) const;
  /**
   * @brief Binds a buffer to the store.
   *
   * Valid only when the store is unbound and has not yet been bound to another buffer. The buffer
   * must be consistent with the mapping policy for the store.  Recommend that the buffer be created
   * by a `create_output_buffer` call.
   *
   * @param buffer Buffer to bind to the store
   *
   * @param extents Extents of the buffer. Passing extents smaller than the actual extents of the
   * buffer is legal; the runtime uses the passed extents as the extents of this store.
   *
   */
  template <typename T, std::int32_t DIM>
  void bind_data(Buffer<T, DIM>& buffer, const Point<DIM>& extents) const;
  /**
   * @brief Binds a 1D buffer of byte-size elements to the store in an untyped manner.
   *
   * Values in the buffer are reinterpreted based on the store's actual type. The buffer must have
   * enough bytes to be aligned on the store's element boundary. For example, a 1D buffer of size
   * 4 wouldn't be valid if the store had the int64 type, whereas it would be if the store's element
   * type is int32.
   *
   * Like the typed counterpart (i.e., bind_data), the operation is legal only when the store is
   * unbound and has not yet been bound to another buffer. The memory in which the buffer is created
   * must be the same as the mapping decision of this store.
   *
   * Can be used only with 1D unbound stores.
   *
   * @param buffer Buffer to bind to the store
   *
   * @param extents Extents of the buffer. Passing extents smaller than the actual extents of the
   * buffer is legal; the runtime uses the passed extents as the extents of this store. The size of
   * the buffer must be at least as big as `extents * type().size()`.
   *
   * @snippet unit/physical_store.cc Bind an untyped buffer to an unbound store
   */
  void bind_untyped_data(Buffer<int8_t, 1>& buffer, const Point<1>& extents) const;
  /**
   * @brief Makes the unbound store empty.
   *
   * Valid only when the store is unbound and has not yet been bound to another buffer.
   */
  void bind_empty_data() const;

  /**
   * @brief Returns the dimension of the store
   *
   * @return The store's dimension
   */
  [[nodiscard]] std::int32_t dim() const;
  /**
   * @brief Returns the type metadata of the store
   *
   * @return The store's type metadata
   */
  [[nodiscard]] Type type() const;
  /**
   * @brief Returns the type code of the store
   *
   * @return The store's type code
   */
  template <typename TYPE_CODE = Type::Code>
  [[nodiscard]] TYPE_CODE code() const;

  /**
   * @brief Returns the store's domain
   *
   * @return Store's domain
   */
  template <std::int32_t DIM>
  [[nodiscard]] Rect<DIM> shape() const;
  /**
   * @brief Returns the store's domain in a dimension-erased domain type
   *
   * @return Store's domain in a dimension-erased domain type
   */
  [[nodiscard]] Domain domain() const;
  /**
   * @brief Returns a raw pointer and strides to the allocation
   *
   * @return An `InlineAllocation` object holding a raw pointer and strides
   */
  [[nodiscard]] InlineAllocation get_inline_allocation() const;
  /**
   * @brief Returns the kind of memory where this physical store resides
   *
   * @return The memory kind
   *
   * @throw std::invalid_argument If this function is called on an unbound store
   */
  [[nodiscard]] mapping::StoreTarget target() const;

  /**
   * @brief Indicates whether the store can have a read accessor
   *
   * @return true The store can have a read accessor
   * @return false The store cannot have a read accesor
   */
  [[nodiscard]] bool is_readable() const;
  /**
   * @brief Indicates whether the store can have a write accessor
   *
   * @return true The store can have a write accessor
   * @return false The store cannot have a write accesor
   */
  [[nodiscard]] bool is_writable() const;
  /**
   * @brief Indicates whether the store can have a reduction accessor
   *
   * @return true The store can have a reduction accessor
   * @return false The store cannot have a reduction accesor
   */
  [[nodiscard]] bool is_reducible() const;

  /**
   * @brief Indicates whether the store is valid.
   *
   * A store passed to a task can be invalid only for reducer tasks for tree reduction.
   *
   * @return true The store is valid
   * @return false The store is invalid and cannot be used in any data access
   */
  [[nodiscard]] bool valid() const;
  /**
   * @brief Indicates whether the store is transformed in any way.
   *
   * @return true The store is transformed
   * @return false The store is not transformed
   */
  [[nodiscard]] bool transformed() const;

  /**
   * @brief Indicates whether the store is backed by a future
   * (i.e., a container for scalar value)
   *
   * @return true The store is backed by a future
   * @return false The store is backed by a region field
   */
  [[nodiscard]] bool is_future() const;
  /**
   * @brief Indicates whether the store is an unbound store.
   *
   * The value DOES NOT indicate that the store has already assigned to a buffer; i.e., the store
   * may have been assigned to a buffer even when this function returns `true`.
   *
   * @return true The store is an unbound store
   * @return false The store is a normal store
   */
  [[nodiscard]] bool is_unbound_store() const;

  /**
   * @brief Constructs a store out of an array
   *
   * @throw std::invalid_argument If the array is nullable or has sub-arrays
   */
  // NOLINTNEXTLINE(google-explicit-constructor) very common pattern in cuNumeric
  PhysicalStore(const PhysicalArray& array);

  LEGATE_CYTHON_DEFAULT_CTOR(PhysicalStore);

  explicit PhysicalStore(InternalSharedPtr<detail::PhysicalStore> impl);

  [[nodiscard]] const SharedPtr<detail::PhysicalStore>& impl() const;

 private:
  void check_accessor_dimension_(std::int32_t dim) const;
  void check_buffer_dimension_(std::int32_t dim) const;
  void check_shape_dimension_(std::int32_t dim) const;
  void check_valid_binding_(bool bind_buffer) const;
  void check_write_access_() const;
  void check_reduction_access_() const;
  template <typename T>
  void check_accessor_type_() const;
  [[nodiscard]] Legion::DomainAffineTransform get_inverse_transform_() const;

  void get_region_field_(Legion::PhysicalRegion& pr, Legion::FieldID& fid) const;
  [[nodiscard]] std::int32_t get_redop_id_() const;
  template <typename ACC, typename T, std::int32_t DIM>
  [[nodiscard]] ACC create_field_accessor_(const Rect<DIM>& bounds) const;
  template <typename ACC, typename T, std::int32_t DIM>
  [[nodiscard]] ACC create_reduction_accessor_(const Rect<DIM>& bounds) const;

  [[nodiscard]] bool is_read_only_future_() const;
  [[nodiscard]] std::size_t get_field_offset_() const;
  [[nodiscard]] const void* get_untyped_pointer_from_future_() const;
  [[nodiscard]] const Legion::Future& get_future_() const;
  [[nodiscard]] const Legion::UntypedDeferredValue& get_buffer_() const;

  void get_output_field_(Legion::OutputRegion& out, Legion::FieldID& fid) const;
  void update_num_elements_(std::size_t num_elements) const;

  SharedPtr<detail::PhysicalStore> impl_{};
};

#undef LEGATE_CORE_TRUE_WHEN_DEBUG

}  // namespace legate

#include "core/data/physical_store.inl"
