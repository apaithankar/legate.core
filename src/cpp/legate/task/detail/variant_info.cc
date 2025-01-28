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

#include <legate/task/detail/variant_info.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <cstdint>

namespace legate::detail {

std::string VariantInfo::to_string() const
{
  return fmt::format("{:x}, {}", reinterpret_cast<std::uintptr_t>(body), fmt::streamed(options));
}

}  // namespace legate::detail
