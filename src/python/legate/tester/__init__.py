# SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

"""Utilities and helpers for implementing the Legate custom test runner."""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Literal, TypeAlias

if TYPE_CHECKING:
    from ..util.types import ArgList

#: Define the available feature types for tests
FeatureType: TypeAlias = Literal["cpus", "cuda", "eager", "openmp"]


@dataclass
class CustomTest:
    file: str
    args: ArgList | None = None
    kind: FeatureType | list[FeatureType] | None = None


def _compute_last_failed_filename() -> str:
    base_name = ".legate-test-last-failed"
    if (legate_dir := os.environ.get("LEGATE_DIR", "")) and (
        legate_arch := os.environ.get("LEGATE_ARCH", "")
    ):
        arch_dir = Path(legate_dir) / legate_arch
        if arch_dir.exists():
            return str(arch_dir / base_name)

    return base_name


#: Location to store a list of last-failed tests
#:
#: Client test scripts can update this value with their own customizations.
LAST_FAILED_FILENAME: str = _compute_last_failed_filename()

del _compute_last_failed_filename
