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

#include "core/data/detail/shape.h"

#include "core/data/shape.h"
#include "core/runtime/detail/runtime.h"
#include "core/utilities/detail/tuple.h"

namespace legate::detail {

Shape::Shape(tuple<std::uint64_t>&& extents)
  : state_{State::READY},
    dim_{static_cast<std::uint32_t>(extents.size())},
    extents_{std::move(extents)}
{
}

const tuple<std::uint64_t>& Shape::extents()
{
  switch (state_) {
    case State::UNBOUND: {
      ensure_binding_();
      [[fallthrough]];
    }
    case State::BOUND: {
      const auto runtime = Runtime::get_runtime();
      auto domain        = runtime->get_index_space_domain(index_space_);
      extents_           = from_domain(domain);
      state_             = State::READY;
      break;
    }
    case State::READY: {
      break;
    }
  }
  return extents_;
}

const Legion::IndexSpace& Shape::index_space()
{
  ensure_binding_();
  if (!index_space_.exists()) {
    LEGATE_CHECK(State::READY == state_);
    index_space_ = Runtime::get_runtime()->find_or_create_index_space(extents_);
  }
  return index_space_;
}

void Shape::set_index_space(const Legion::IndexSpace& index_space)
{
  LEGATE_CHECK(State::UNBOUND == state_);
  index_space_ = index_space;
  state_       = State::BOUND;
}

void Shape::copy_extents_from(const Shape& other)
{
  LEGATE_CHECK(State::BOUND == state_);
  LEGATE_ASSERT(dim_ == other.dim_);
  LEGATE_ASSERT(index_space_ == other.index_space_);
  state_   = State::READY;
  extents_ = other.extents_;
}

std::string Shape::to_string() const
{
  switch (state_) {
    case State::UNBOUND: {
      return "Shape(unbound " + std::to_string(dim_) + "D)";
    }
    case State::BOUND: {
      return "Shape(bound " + std::to_string(dim_) + "D)";
    }
    case State::READY: {
      return "Shape" + extents_.to_string();
    }
  }
  return "";
}

bool Shape::operator==(Shape& other)
{
  if (this == &other) {
    return true;
  }
  if (State::UNBOUND == state_ || State::UNBOUND == other.state_) {
    Runtime::get_runtime()->flush_scheduling_window();
    if (State::UNBOUND == state_ || State::UNBOUND == other.state_) {
      throw std::invalid_argument{"Illegal to access an uninitialized unbound store"};
    }
  }
  // If both shapes are in the bound state and their index spaces are the same, we can elide the
  // blocking equivalence check
  if (State::BOUND == state_ && State::BOUND == other.state_ &&
      index_space_ == other.index_space_) {
    return true;
  }
  // Otherwise, we have no choice but block waiting on the exact extents
  return extents() == other.extents();
}

void Shape::ensure_binding_()
{
  if (State::UNBOUND != state_) {
    return;
  }
  Runtime::get_runtime()->flush_scheduling_window();
  if (State::UNBOUND == state_) {
    throw std::invalid_argument{"Illegal to access an uninitialized unbound store"};
  }
}

}  // namespace legate::detail
