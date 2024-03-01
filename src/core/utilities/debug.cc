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

#include "core/utilities/debug.h"

#include "core/type/type_traits.h"
#include "core/utilities/dispatch.h"

namespace legate {

namespace {  // anonymous

class print_dense_array_fn {
 public:
  template <Type::Code CODE, int DIM>
  [[nodiscard]] std::string operator()(const PhysicalStore& store) const
  {
    using T              = type_of<CODE>;
    const Rect<DIM> rect = store.shape<DIM>();
    return print_dense_array(store.read_accessor<T>(rect), rect);
  }
};

}  // namespace

std::string print_dense_array(const PhysicalStore& store)
{
  LegateCheck(store.is_readable());
  return double_dispatch(store.dim(), store.code(), print_dense_array_fn{}, store);
}

}  // namespace legate
