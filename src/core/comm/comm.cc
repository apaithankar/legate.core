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

#include "legate_defines.h"
//
#include "core/comm/comm.h"
#include "core/comm/comm_cal.h"
#include "core/comm/comm_cpu.h"
#include "core/comm/comm_nccl.h"
#include "core/runtime/runtime.h"
#include "core/utilities/env.h"

#include "env_defaults.h"

namespace legate::comm {

void register_tasks(const detail::Library* library)
{
  if (LegateDefined(LEGATE_USE_CUDA)) {
    nccl::register_tasks(library);
  }
  if (LegateDefined(LEGATE_USE_CAL)) {
    cal::register_tasks(library);
  }
  const bool disable_mpi = LEGATE_DISABLE_MPI.get(DISABLE_MPI_DEFAULT, DISABLE_MPI_TEST);
  if (!disable_mpi) {
    cpu::register_tasks(library);
  }
}

void register_builtin_communicator_factories(const detail::Library* library)
{
  if (LegateDefined(LEGATE_USE_CUDA)) {
    nccl::register_factory(library);
  }
  cpu::register_factory(library);
  if (LegateDefined(LEGATE_USE_CAL)) {
    cal::register_factory(library);
  }
}

}  // namespace legate::comm
