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

#include "core/data/shape.h"
#include "core/partitioning/constraint.h"
#include "core/utilities/internal_shared_ptr.h"
#include "core/utilities/tuple.h"

#include <string>
#include <utility>
#include <vector>

namespace legate {
class Partition;
}  // namespace legate

namespace legate::detail {
class Operation;
class Strategy;

class LogicalStore;
class Variable;

class Expr {
 public:
  enum class Kind : std::uint8_t {
    LITERAL  = 0,
    VARIABLE = 1,
  };

  Expr()                           = default;
  virtual ~Expr()                  = default;
  Expr(const Expr&)                = default;
  Expr(Expr&&) noexcept            = default;
  Expr& operator=(const Expr&)     = default;
  Expr& operator=(Expr&&) noexcept = default;

  virtual void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const = 0;
  [[nodiscard]] virtual bool closed() const                                                  = 0;
  [[nodiscard]] virtual std::string to_string() const                                        = 0;
  [[nodiscard]] virtual Kind kind() const                                                    = 0;
};

class Literal final : public Expr {
 public:
  explicit Literal(InternalSharedPtr<Partition> partition);

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  [[nodiscard]] bool closed() const override;
  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] Kind kind() const override;

  [[nodiscard]] const InternalSharedPtr<Partition>& partition() const;

 private:
  InternalSharedPtr<Partition> partition_{};
};

class Variable final : public Expr {
 public:
  Variable(const Operation* op, std::int32_t id);

  friend bool operator==(const Variable& lhs, const Variable& rhs);

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  [[nodiscard]] bool closed() const override;
  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] Kind kind() const override;

  [[nodiscard]] const Operation* operation() const;
  [[nodiscard]] const InternalSharedPtr<LogicalStore>& store() const;

  [[nodiscard]] std::size_t hash() const noexcept;

 private:
  const Operation* op_{};
  std::int32_t id_{};
};

class Constraint {
 public:
  enum class Kind : std::uint8_t {
    ALIGNMENT = 0,
    BROADCAST = 1,
    IMAGE     = 2,
    SCALE     = 3,
    BLOAT     = 4,
  };

  virtual ~Constraint() = default;
  virtual void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const = 0;
  [[nodiscard]] virtual std::string to_string() const                                        = 0;
  [[nodiscard]] virtual Kind kind() const                                                    = 0;
  virtual void validate() const                                                              = 0;
};

class Alignment final : public Constraint {
 public:
  Alignment(const Variable* lhs, const Variable* rhs);

  [[nodiscard]] Kind kind() const override;

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  void validate() const override;

  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] const Variable* lhs() const;
  [[nodiscard]] const Variable* rhs() const;

  [[nodiscard]] bool is_trivial() const;

 private:
  const Variable* lhs_{};
  const Variable* rhs_{};
};

class Broadcast final : public Constraint {
 public:
  explicit Broadcast(const Variable* variable);

  Broadcast(const Variable* variable, tuple<std::uint32_t> axes);

  [[nodiscard]] Kind kind() const override;

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  void validate() const override;

  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] const Variable* variable() const;
  [[nodiscard]] const tuple<std::uint32_t>& axes() const;

 private:
  const Variable* variable_{};
  // Broadcast all dimensions when empty
  tuple<std::uint32_t> axes_{};
};

class ImageConstraint final : public Constraint {
 public:
  ImageConstraint(const Variable* var_function,
                  const Variable* var_range,
                  ImageComputationHint hint);

  [[nodiscard]] Kind kind() const override;

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  void validate() const override;

  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] const Variable* var_function() const;
  [[nodiscard]] const Variable* var_range() const;

  [[nodiscard]] InternalSharedPtr<Partition> resolve(const Strategy& strategy) const;

 private:
  const Variable* var_function_{};
  const Variable* var_range_{};
  ImageComputationHint hint_{};
};

class ScaleConstraint final : public Constraint {
 public:
  ScaleConstraint(tuple<std::uint64_t> factors,
                  const Variable* var_smaller,
                  const Variable* var_bigger);

  [[nodiscard]] Kind kind() const override;

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  void validate() const override;

  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] const Variable* var_smaller() const;
  [[nodiscard]] const Variable* var_bigger() const;

  [[nodiscard]] InternalSharedPtr<Partition> resolve(const Strategy& strategy) const;

 private:
  tuple<std::uint64_t> factors_{};
  const Variable* var_smaller_{};
  const Variable* var_bigger_{};
};

class BloatConstraint final : public Constraint {
 public:
  BloatConstraint(const Variable* var_source,
                  const Variable* var_bloat,
                  tuple<std::uint64_t> low_offsets,
                  tuple<std::uint64_t> high_offsets);

  [[nodiscard]] Kind kind() const override;

  void find_partition_symbols(std::vector<const Variable*>& partition_symbols) const override;

  void validate() const override;

  [[nodiscard]] std::string to_string() const override;

  [[nodiscard]] const Variable* var_source() const;
  [[nodiscard]] const Variable* var_bloat() const;

  [[nodiscard]] InternalSharedPtr<Partition> resolve(const Strategy& strategy) const;

 private:
  const Variable* var_source_{};
  const Variable* var_bloat_{};
  tuple<std::uint64_t> low_offsets_{};
  tuple<std::uint64_t> high_offsets_{};
};

[[nodiscard]] InternalSharedPtr<Alignment> align(const Variable* lhs, const Variable* rhs);

[[nodiscard]] InternalSharedPtr<Broadcast> broadcast(const Variable* variable);

[[nodiscard]] InternalSharedPtr<Broadcast> broadcast(const Variable* variable,
                                                     tuple<std::uint32_t> axes);

[[nodiscard]] InternalSharedPtr<ImageConstraint> image(const Variable* var_function,
                                                       const Variable* var_range,
                                                       ImageComputationHint hint);

[[nodiscard]] InternalSharedPtr<ScaleConstraint> scale(tuple<std::uint64_t> factors,
                                                       const Variable* var_smaller,
                                                       const Variable* var_bigger);

[[nodiscard]] InternalSharedPtr<BloatConstraint> bloat(const Variable* var_source,
                                                       const Variable* var_bloat,
                                                       tuple<std::uint64_t> low_offsets,
                                                       tuple<std::uint64_t> high_offsets);

}  // namespace legate::detail

#include "core/partitioning/detail/constraint.inl"
