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

#include "legate/runtime/detail/communicator_manager.h"

namespace legate::detail {

template <class Desc>
mapping::detail::Machine CommunicatorFactory::CacheKey<Desc>::get_machine() const
{
  return mapping::detail::Machine({{target, range}});
}

template <class Desc>
bool CommunicatorFactory::CacheKey<Desc>::operator==(const CacheKey& other) const
{
  return desc == other.desc && target == other.target && range == other.range;
}

template <class Desc>
std::size_t CommunicatorFactory::CacheKey<Desc>::hash() const noexcept
{
  return hash_all(desc, target, range);
}

}  // namespace legate::detail
