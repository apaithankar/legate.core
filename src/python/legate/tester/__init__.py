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

"""Utilities and helpers for implementing the Legate custom test runner.

"""
from __future__ import annotations

import os

from dataclasses import dataclass
from typing import Literal, TypeAlias

from ..util.types import ArgList

#: Define the available feature types for tests
FeatureType: TypeAlias = Literal["cpus", "cuda", "eager", "openmp"]


@dataclass
class CustomTest:
    file: str
    args: ArgList | None = None
    kind: FeatureType | list[FeatureType] | None = None


# These will eventually go away but it will take a few PRs back and
# forth to avoid breaking downstream usage
SKIPPED_EXAMPLES: set[str] = set()
PER_FILE_ARGS: dict[str, ArgList] = {}
CUSTOM_FILES: list[CustomTest] = []


def _compute_last_failed_filename() -> str:
    base_name = ".legate-test-last-failed"
    if (legate_dir := os.environ.get("LEGATE_DIR", "")) and (
        legate_arch := os.environ.get("LEGATE_ARCH", "")
    ):
        arch_dir = os.path.join(legate_dir, legate_arch)
        if os.path.exists(arch_dir):
            return os.path.join(arch_dir, base_name)

    return base_name


#: Location to store a list of last-failed tests
#:
#: Client test scripts can update this value with their own customizations.
LAST_FAILED_FILENAME: str = _compute_last_failed_filename()

del _compute_last_failed_filename
