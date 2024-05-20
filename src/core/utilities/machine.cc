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

#include "core/utilities/machine.h"

#include "core/runtime/detail/config.h"
#include "core/runtime/detail/runtime.h"

namespace legate {

Memory::Kind find_memory_kind_for_executing_processor(bool host_accessible)
{
  if (!Legion::Runtime::has_runtime()) {
    throw std::runtime_error{"Runtime has not started"};
  }

  switch (const auto kind = detail::Runtime::get_runtime()->get_executing_processor().kind()) {
    case Processor::Kind::LOC_PROC: return Memory::Kind::SYSTEM_MEM;
    case Processor::Kind::TOC_PROC:
      return host_accessible ? Memory::Kind::Z_COPY_MEM : Memory::Kind::GPU_FB_MEM;
    case Processor::Kind::OMP_PROC:
      return detail::Config::has_socket_mem ? Memory::Kind::SOCKET_MEM : Memory::Kind::SYSTEM_MEM;
    case Processor::Kind::PY_PROC: return Memory::Kind::SYSTEM_MEM;
    default: LEGATE_ABORT("Unknown processor kind " << kind);
  }
  return Memory::Kind::SYSTEM_MEM;
}

}  // namespace legate
