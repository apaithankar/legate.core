# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
from __future__ import annotations

# Use of star import is deliberate, want to import all the other build hooks
# that we don't override. As a plus, this also keeps us future-proof
from scikit_build_core.build import *  # type: ignore[import-not-found] # noqa: F403

from .build import build_wheel

# override with our implementations
from .editable import build_editable
