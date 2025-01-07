# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

"""Default configuration values."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from . import FeatureType

# -- core

#: Value to use if --cpus is not specified.
CPUS_PER_NODE = 2

#: Value to use if --gpus is not specified.
GPUS_PER_NODE = 1

#: Value to use if --omps is not specified.
OMPS_PER_NODE = 1

#: Value to use if --ompthreads is not specified.
OMPTHREADS = 4

# -- memory

# Value to use if --fbmem is not specified (MB)
GPU_MEMORY_BUDGET = 4096

# Value to use if --sysmem is not specified (MB)
SYS_MEMORY_BUDGET = 4000

#: Value to use if --numamem is not specified.
NUMA_MEMORY_BUDGET = 4000

# -- multi_node

#: Value to use if --nodes is not specified
NODES = 1

#: Value to use if --ranks-per-node is not specified.
RANKS_PER_NODE = 1

# --

# names for available feature stages
FEATURES: tuple[FeatureType, ...] = ("cpus", "cuda", "eager", "openmp")

# Value to use if --bloat-factor is not specified
GPU_BLOAT_FACTOR = 1.5

# Value to use if --gpu-delay is not specified. (ms)
GPU_DELAY = 2000

# internal defaults

# Default values to apply to normalize the testing environment.
PROCESS_ENV = {
    "LEGATE_TEST": "1",
    "LEGATE_CONSENSUS": "1",
    # TODO(mpapadakis): We do this so that gtest can capture the output on
    # death tests. We will no longer need to do this once
    # https://github.com/StanfordLegion/legion/issues/1711 is fixed.
    "LEGION_DEFAULT_ARGS": "-logfile stderr",
}

# sysmem value to use for non-CPU stages
SMALL_SYSMEM = 300
