/*
 * SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES. All rights
 * reserved. SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#pragma once

#include <legate/utilities/memory.h>

namespace legate {

template <typename T>
void DefaultDelete<T>::operator()(T* ptr) const noexcept
{
  // NOLINTNEXTLINE(bugprone-sizeof-expression): comparing with 0 is the whole point here
  static_assert(sizeof(T) > 0, "default_delete cannot be instantiated for incomplete type");
  std::default_delete<T>{}(ptr);
}

}  // namespace legate
