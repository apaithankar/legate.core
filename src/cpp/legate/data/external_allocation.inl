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

#include <legate/data/external_allocation.h>

namespace legate {

inline ExternalAllocation::ExternalAllocation(InternalSharedPtr<detail::ExternalAllocation>&& impl)
  : impl_{std::move(impl)}
{
}

inline const SharedPtr<detail::ExternalAllocation>& ExternalAllocation::impl() const
{
  return impl_;
}

}  // namespace legate
