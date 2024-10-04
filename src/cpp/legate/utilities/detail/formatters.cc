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

#include "legate/utilities/detail/formatters.h"

#include "legate/data/detail/logical_region_field.h"
#include "legate/data/detail/logical_store.h"
#include "legate/data/detail/shape.h"
#include "legate/operation/detail/operation.h"
#include "legate/partitioning/constraint.h"
#include "legate/partitioning/detail/constraint.h"
#include "legate/type/detail/type_info.h"
#include "legate/utilities/detail/zstring_view.h"
#include "legate/utilities/macros.h"
#include "legate/utilities/typedefs.h"

#include <fmt/format.h>

namespace fmt {

format_context::iterator formatter<legate::detail::Type>::format(const legate::detail::Type& a,
                                                                 format_context& ctx) const
{
  return formatter<std::string>::format(a.to_string(), ctx);
}

format_context::iterator formatter<legate::detail::Operation>::format(
  const legate::detail::Operation& op, format_context& ctx) const
{
  return formatter<std::string>::format(op.to_string(), ctx);
}

format_context::iterator formatter<legate::detail::Shape>::format(
  const legate::detail::Shape& shape, format_context& ctx) const
{
  return formatter<std::string>::format(shape.to_string(), ctx);
}

format_context::iterator formatter<legate::detail::Constraint>::format(
  const legate::detail::Constraint& constraint, format_context& ctx) const
{
  return formatter<std::string>::format(constraint.to_string(), ctx);
}

format_context::iterator formatter<legate::detail::Variable>::format(
  const legate::detail::Variable& var, format_context& ctx) const
{
  return formatter<std::string>::format(var.to_string(), ctx);
}

format_context::iterator formatter<legate::VariantCode>::format(legate::VariantCode variant,
                                                                format_context& ctx) const
{
  string_view name = "(unknown)";

  switch (variant) {
#define LEGATE_VARIANT_CASE(x) \
  case legate::VariantCode::x: name = #x "_VARIANT"; break
    LEGATE_VARIANT_CASE(NONE);
    LEGATE_VARIANT_CASE(CPU);
    LEGATE_VARIANT_CASE(GPU);
    LEGATE_VARIANT_CASE(OMP);
#undef LEGATE_VARIANT_CASE
  }

  return formatter<string_view>::format(name, ctx);
}

format_context::iterator formatter<legate::LocalTaskID>::format(legate::LocalTaskID id,
                                                                format_context& ctx) const
{
  return formatter<std::underlying_type_t<legate::LocalTaskID>>::format(fmt::underlying(id), ctx);
}

format_context::iterator formatter<legate::GlobalTaskID>::format(legate::GlobalTaskID id,
                                                                 format_context& ctx) const
{
  return formatter<std::underlying_type_t<legate::GlobalTaskID>>::format(fmt::underlying(id), ctx);
}

format_context::iterator formatter<legate::LocalRedopID>::format(legate::LocalRedopID id,
                                                                 format_context& ctx) const
{
  return formatter<std::underlying_type_t<legate::LocalRedopID>>::format(fmt::underlying(id), ctx);
}

format_context::iterator formatter<legate::GlobalRedopID>::format(legate::GlobalRedopID id,
                                                                  format_context& ctx) const
{
  return formatter<std::underlying_type_t<legate::GlobalRedopID>>::format(fmt::underlying(id), ctx);
}

format_context::iterator formatter<legate::ImageComputationHint>::format(
  legate::ImageComputationHint hint, format_context& ctx) const
{
  string_view name = "(unknown)";
  switch (hint) {
#define LEGATE_HINT_CASE(x) \
  case legate::ImageComputationHint::x: name = LEGATE_STRINGIZE_(x); break
    LEGATE_HINT_CASE(NO_HINT);
    LEGATE_HINT_CASE(MIN_MAX);
    LEGATE_HINT_CASE(FIRST_LAST);
#undef LEGATE_HINT_CASE
  };

  return formatter<string_view>::format(name, ctx);
}

format_context::iterator formatter<legate::detail::ZStringView>::format(
  const legate::detail::ZStringView& sv, format_context& ctx) const
{
  return formatter<string_view>::format(sv.as_string_view(), ctx);
}

format_context::iterator formatter<legate::detail::LogicalRegionField>::format(
  const legate::detail::LogicalRegionField& lrf, format_context& ctx) const
{
  return formatter<std::string>::format(lrf.to_string(), ctx);
}

format_context::iterator formatter<legate::detail::Storage>::format(
  const legate::detail::Storage& s, format_context& ctx) const
{
  return formatter<std::string>::format(s.to_string(), ctx);
}

}  // namespace fmt
