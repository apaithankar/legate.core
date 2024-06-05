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
#include "core/utilities/internal_shared_ptr.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace legate::detail {

class BufferBuilder;
class FixedArrayType;
class ListType;
class StructType;

class Type {
 public:
  using Code = legate::Type::Code;

  explicit Type(Code type_code);

  virtual ~Type() = default;
  [[nodiscard]] virtual std::uint32_t size() const;
  [[nodiscard]] virtual std::uint32_t alignment() const = 0;
  [[nodiscard]] virtual std::uint32_t uid() const       = 0;
  [[nodiscard]] virtual bool variable_size() const      = 0;
  [[nodiscard]] virtual std::string to_string() const   = 0;
  [[nodiscard]] virtual bool is_primitive() const       = 0;
  virtual void pack(BufferBuilder& buffer) const        = 0;
  [[nodiscard]] virtual const FixedArrayType& as_fixed_array_type() const;
  [[nodiscard]] virtual const StructType& as_struct_type() const;
  [[nodiscard]] virtual const ListType& as_list_type() const;
  [[nodiscard]] virtual bool equal(const Type& other) const = 0;

  void record_reduction_operator(std::int32_t op_kind, std::int32_t global_op_id) const;
  [[nodiscard]] std::int32_t find_reduction_operator(std::int32_t op_kind) const;
  [[nodiscard]] std::int32_t find_reduction_operator(ReductionOpKind op_kind) const;
  bool operator==(const Type& other) const;
  bool operator!=(const Type& other) const;

  Code code;
};

class PrimitiveType final : public Type {
 public:
  explicit PrimitiveType(Code type_code);

  [[nodiscard]] std::uint32_t size() const override;
  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] std::uint32_t uid() const override;
  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::string to_string() const override;
  [[nodiscard]] bool is_primitive() const override;
  void pack(BufferBuilder& buffer) const override;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;

  std::uint32_t size_{};
  std::uint32_t alignment_{};
};

class StringType final : public Type {
 public:
  StringType();

  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] std::uint32_t uid() const override;
  [[nodiscard]] std::string to_string() const override;
  [[nodiscard]] bool is_primitive() const override;
  void pack(BufferBuilder& buffer) const override;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;
};

class ExtensionType : public Type {
 public:
  ExtensionType(std::uint32_t uid, Type::Code type_code);
  [[nodiscard]] std::uint32_t uid() const override;
  [[nodiscard]] bool is_primitive() const override;

 protected:
  std::uint32_t uid_{};
};

class BinaryType final : public ExtensionType {
 public:
  BinaryType(std::uint32_t uid, std::uint32_t size);

  [[nodiscard]] std::uint32_t size() const override;
  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::string to_string() const override;
  void pack(BufferBuilder& buffer) const override;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;

  std::uint32_t size_{};
};

class FixedArrayType final : public ExtensionType {
 public:
  FixedArrayType(std::uint32_t uid, InternalSharedPtr<Type> element_type, std::uint32_t N);

  [[nodiscard]] std::uint32_t size() const override;
  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::string to_string() const override;
  void pack(BufferBuilder& buffer) const override;
  [[nodiscard]] const FixedArrayType& as_fixed_array_type() const override;

  [[nodiscard]] std::uint32_t num_elements() const;
  [[nodiscard]] const InternalSharedPtr<Type>& element_type() const;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;

  InternalSharedPtr<Type> element_type_{};
  // clang-tidy wants us to lower-case this, but that would make it less readable.
  std::uint32_t N_{};  // NOLINT(readability-identifier-naming)
  std::uint32_t size_{};
};

class StructType final : public ExtensionType {
 public:
  StructType(std::uint32_t uid,
             std::vector<InternalSharedPtr<Type>>&& field_types,
             bool align = false);

  [[nodiscard]] std::uint32_t size() const override;
  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::string to_string() const override;
  void pack(BufferBuilder& buffer) const override;
  [[nodiscard]] const StructType& as_struct_type() const override;

  [[nodiscard]] std::uint32_t num_fields() const;
  [[nodiscard]] InternalSharedPtr<Type> field_type(std::uint32_t field_idx) const;
  [[nodiscard]] const std::vector<InternalSharedPtr<Type>>& field_types() const;
  [[nodiscard]] bool aligned() const;
  [[nodiscard]] const std::vector<std::uint32_t>& offsets() const;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;

  bool aligned_{};
  std::uint32_t size_{};
  std::uint32_t alignment_{};
  std::vector<InternalSharedPtr<Type>> field_types_{};
  std::vector<std::uint32_t> offsets_{};
};

class ListType final : public ExtensionType {
 public:
  ListType(std::uint32_t uid, InternalSharedPtr<Type> element_type);

  [[nodiscard]] std::uint32_t alignment() const override;
  [[nodiscard]] bool variable_size() const override;
  [[nodiscard]] std::string to_string() const override;
  void pack(BufferBuilder& buffer) const override;
  [[nodiscard]] const ListType& as_list_type() const override;

  [[nodiscard]] const InternalSharedPtr<Type>& element_type() const;

 private:
  [[nodiscard]] bool equal(const Type& other) const override;

  InternalSharedPtr<Type> element_type_{};
};

[[nodiscard]] InternalSharedPtr<Type> primitive_type(Type::Code code);

[[nodiscard]] InternalSharedPtr<Type> string_type();

[[nodiscard]] InternalSharedPtr<Type> binary_type(std::uint32_t size);

[[nodiscard]] InternalSharedPtr<Type> fixed_array_type(InternalSharedPtr<Type> element_type,
                                                       std::uint32_t N);

[[nodiscard]] InternalSharedPtr<Type> struct_type(std::vector<InternalSharedPtr<Type>> field_types,
                                                  bool align);

[[nodiscard]] InternalSharedPtr<Type> list_type(InternalSharedPtr<Type> element_type);

[[nodiscard]] InternalSharedPtr<Type> bool_();  // NOLINT(readability-identifier-naming)
[[nodiscard]] InternalSharedPtr<Type> int8();
[[nodiscard]] InternalSharedPtr<Type> int16();
[[nodiscard]] InternalSharedPtr<Type> int32();
[[nodiscard]] InternalSharedPtr<Type> int64();
[[nodiscard]] InternalSharedPtr<Type> uint8();
[[nodiscard]] InternalSharedPtr<Type> uint16();
[[nodiscard]] InternalSharedPtr<Type> uint32();
[[nodiscard]] InternalSharedPtr<Type> uint64();
[[nodiscard]] InternalSharedPtr<Type> float16();
[[nodiscard]] InternalSharedPtr<Type> float32();
[[nodiscard]] InternalSharedPtr<Type> float64();
[[nodiscard]] InternalSharedPtr<Type> complex64();
[[nodiscard]] InternalSharedPtr<Type> complex128();
[[nodiscard]] InternalSharedPtr<Type> point_type(std::uint32_t ndim);
[[nodiscard]] InternalSharedPtr<Type> rect_type(std::uint32_t ndim);
[[nodiscard]] InternalSharedPtr<Type> domain_type();
[[nodiscard]] InternalSharedPtr<Type> null_type();
[[nodiscard]] bool is_point_type(const InternalSharedPtr<Type>& type);
[[nodiscard]] bool is_point_type(const InternalSharedPtr<Type>& type, std::uint32_t ndim);
[[nodiscard]] std::int32_t ndim_point_type(const InternalSharedPtr<Type>& type);
[[nodiscard]] bool is_rect_type(const InternalSharedPtr<Type>& type);
[[nodiscard]] bool is_rect_type(const InternalSharedPtr<Type>& type, std::uint32_t ndim);
[[nodiscard]] std::int32_t ndim_rect_type(const InternalSharedPtr<Type>& type);

}  // namespace legate::detail

#include "core/type/detail/type_info.inl"
