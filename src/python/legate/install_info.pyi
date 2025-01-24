# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

def get_libpath(lib_base_name: str, full_lib_name: str) -> str: ...

LEGATE_ARCH: str

libpath: str

networks: list[str]

max_dim: int

max_fields: int

conduit: str

build_type: str

ON: bool

OFF: bool

use_cuda: bool

use_openmp: bool

legion_version: str

legion_git_branch: str

legion_git_repo: str
