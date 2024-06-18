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

#include <memory>

namespace legate {

/// @brief deleter for using unique_ptr with incomplete types
/// @tparam T the type to delete
/// @code{.cpp}
///  // in header file:
///  struct Foo;
///  extern template class legate::DefaultDelete<Foo>; // Suppress instantiation
///  std::unique_ptr<Foo, DefaultDelete<Foo>> foo;     // OK
///
///  // in source file:
///  struct Foo { int x; };
///  template class legate::DefaultDelete<Foo>;        // Explicit instantiation
/// @endcode
template <typename T>
class DefaultDelete {
 public:
  void operator()(T*) const noexcept;
};

}  // namespace legate

#include "core/utilities/internal_shared_ptr.h"
#include "core/utilities/shared_ptr.h"

#include "core/utilities/memory.inl"
