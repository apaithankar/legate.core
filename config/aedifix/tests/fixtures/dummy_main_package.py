# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES.B
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

from os import environ
from pathlib import Path

from ...package.main_package import MainPackage


class DummyMainPackage(MainPackage):
    @property
    def arch_value(self) -> str:
        return environ[self.arch_name]

    @property
    def project_dir_value(self) -> Path:
        return Path(environ[self.project_dir_name])
