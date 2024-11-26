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
from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING, Final

from ...cmake import CMAKE_VARIABLE, CMakePath, CMakeString
from ...util.argument_parser import ArgSpec, ConfigArgument
from ..package import Package

if TYPE_CHECKING:
    from ...manager import ConfigurationManager


class ZLIB(Package):
    With_ZLIB: Final = ConfigArgument(
        name="--with-zlib",
        spec=ArgSpec(
            dest="with_zlib", type=bool, help="Build with Zlib support."
        ),
    )
    ZLIB_ROOT: Final = ConfigArgument(
        name="--with-zlib-dir",
        spec=ArgSpec(
            dest="zlib_dir",
            type=Path,
            help="Path to ZLIB installation directory.",
        ),
        cmake_var=CMAKE_VARIABLE("ZLIB_ROOT", CMakePath),
    )
    ZLIB_VERSION = CMAKE_VARIABLE("ZLIB_VERSION", CMakeString)
    ZLIB_INCLUDE_DIRS = CMAKE_VARIABLE("ZLIB_INCLUDE_DIRS", CMakePath)
    ZLIB_LIBRARIES = CMAKE_VARIABLE("ZLIB_LIBRARIES", CMakeString)

    def __init__(self, manager: ConfigurationManager) -> None:
        r"""Construct a ZLIB package.

        Parameters
        ----------
        manager : ConfigurationManager
            The configuration manager to manage this package.
        """
        super().__init__(manager=manager, name="ZLIB")

    def configure(self) -> None:
        r"""Configure ZLIB."""
        super().configure()
        if not self.state.enabled():
            return

        self.set_flag_if_user_set(self.ZLIB_ROOT, self.cl_args.zlib_dir)

    def summarize(self) -> str:
        r"""Summarize configured ZLIB.

        Returns
        -------
        summary : str
            The summary of ZLIB
        """
        if not self.state.enabled():
            return ""

        lines = []
        # Some versions of FindZLIB don't actually set these variables in the
        # cache, so we may or may not find them
        if version := self.manager.read_or_get_cmake_variable(
            self.ZLIB_VERSION
        ):
            lines.append(("Version", version))
        if inc_dirs := self.manager.read_or_get_cmake_variable(
            self.ZLIB_INCLUDE_DIRS
        ):
            lines.append(("Include Dirs", inc_dirs))
        elif inc_dir := self.manager.read_or_get_cmake_variable(
            "ZLIB_INCLUDE_DIR"
        ):
            lines.append(("Include Dir", inc_dir))
        if libs := self.manager.read_or_get_cmake_variable(
            self.ZLIB_LIBRARIES
        ):
            lines.append(("Libraries", libs))
        return self.create_package_summary(lines)


def create_package(manager: ConfigurationManager) -> ZLIB:
    return ZLIB(manager)
