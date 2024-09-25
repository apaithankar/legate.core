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

#include "legate/data/logical_store.h"

namespace legate {

inline const SharedPtr<detail::LogicalStore>& LogicalStore::impl() const { return impl_; }

inline const tuple<std::uint64_t>& LogicalStore::extents() const { return shape().extents(); }

// ==========================================================================================

inline const SharedPtr<detail::LogicalStorePartition>& LogicalStorePartition::impl() const
{
  return impl_;
}

}  // namespace legate
