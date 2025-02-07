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

from __future__ import annotations

import os
import re
import sys
import contextlib
from pathlib import Path

from .types import LegatePaths, LegionPaths

__all__ = (
    "get_legate_build_dir",
    "get_legate_paths",
    "get_legion_paths",
    "read_c_define",
    "read_cmake_cache_value",
)


def assert_file_exists(path: Path) -> None:
    assert path.exists()
    assert path.is_file()


def assert_dir_exists(path: Path) -> None:
    assert path.exists()
    assert path.is_dir()


def read_c_define(header_path: Path, name: str) -> str | None:
    """Open a C header file and read the value of a #define.

    Parameters
    ----------
    header_path : Path
        Location of the C header file to scan

    name : str
        The name to search the header for

    Returns
    -------
        str : value from the header or None, if it does not exist

    """
    try:
        with header_path.open() as f:
            lines = (line for line in f if line.startswith("#define"))
            for line in lines:
                tokens = line.split(" ")
                if tokens[1].strip() == name:
                    return tokens[2].strip()
    except OSError:
        pass

    return None


def read_cmake_cache_value(file_path: Path, pattern: str) -> str:
    """Search a cmake cache file for a given pattern and return the associated
    value.

    Parameters
    ----------
        file_path: Path
            Location of the cmake cache file to scan

        pattern : str
            A pattern to seach for in the file

    Returns
    -------
        str

    Raises
    ------
        RuntimeError, if the value is not found

    """
    with file_path.open() as f:
        for line in f:
            if re.match(pattern, line):
                return line.strip().split("=")[1]

    msg = f"Could not find value for {pattern} in {file_path}"
    raise RuntimeError(msg)


def is_legate_path_in_src_tree(path: Path) -> bool:
    r"""Determine whether ``path`` is the path to the in-source legate module.

    Parameters
    ----------
    path : Path
        The path to check.

    Returns
    -------
    bool
        True if path is the in-source legate module, False otherwise.
    """
    ret = tuple(path.parts[-3:]) == ("src", "python", "legate")
    if ret:
        assert_file_exists(path / "CMakeLists.txt")
        assert_file_exists(path.parent / "CMakeLists.txt")
        assert_file_exists(path.parent.parent / "cpp" / "CMakeLists.txt")
    return ret


def get_legate_build_dir_from_arch_build_dir(
    legate_arch_dir: Path,
) -> Path | None:
    r"""Given a path to the legate arch directory, determine the original
    legate build directory.

    Parameters
    ----------
    legate_arch_dir : Path
        The path to the legate arch directory.

    Returns
    -------
    None
        If the build directory cannot be located.
    Path
        The path to the located legate build directory.
    """
    cmake_build_dir = legate_arch_dir / "cmake_build"
    if (cmake_build_dir / "CMakeCache.txt").exists():
        return cmake_build_dir
    return None


def get_legate_build_dir_from_skbuild_dir(skbuild_dir: Path) -> Path | None:
    r"""Given the path to the skbuild directory, determine the original
    legate build directory (i.e. the one containing the original CMake files).

    Parameters
    ----------
    skbuild_dir : Path
        The path to the scikit-build-core build directory.

    Returns
    -------
    None
        If the legate build dir could not be located.
    Path
        The path to the located legate build directory.
    """
    if not skbuild_dir.exists():
        return None

    assert_dir_exists(skbuild_dir)
    cmake_cache_txt = skbuild_dir / "CMakeCache.txt"
    if not cmake_cache_txt.exists():
        msg = f"scikit-build-core CMakeCache does not exist: {cmake_cache_txt}"
        raise RuntimeError(msg)

    legate_found_method = read_cmake_cache_value(
        cmake_cache_txt, "_legate_FOUND_METHOD:INTERNAL="
    )
    match legate_found_method:
        case "SELF_BUILT":
            return skbuild_dir
        case "PRE_BUILT":
            # Use of legate_DIR vs LEGATE_DIR is deliberate. The latter points
            # to the "base" directory, the former will point to the arch
            # directory (since that's where the pre-built version lives).
            legate_cmake_build_dir = Path(
                read_cmake_cache_value(cmake_cache_txt, "legate_DIR:PATH=")
            )
            assert_dir_exists(legate_cmake_build_dir)
            assert_file_exists(legate_cmake_build_dir / "CMakeCache.txt")
            return legate_cmake_build_dir
        case _:
            m = (
                "Unknown legate found method: "
                f"{legate_found_method} in {cmake_cache_txt}"
            )
            raise ValueError(m)


def get_legate_build_dir(legate_parent_dir: Path) -> Path | None:
    """Determine the location of the Legate build directory.

    If the build directory cannot be found, None is returned.

    Parameters
    ----------
    legate_parent_dir : Path
        Directory containing the legate Python module, i.e. if given
        '/path/to/legate/__init__.py', legate_dir is '/path/to'.


    Returns
    -------
    Path or None
    """

    def get_legate_arch() -> str | None:
        # We might be calling this from the driver (i.e. legate) in which case
        # we don't want to require the user to have this set.
        if legate_arch := os.environ.get("LEGATE_ARCH", "").strip():
            return legate_arch

        # User has LEGATE_ARCH undefined, but we may yet still be an editable
        # installation.
        if is_legate_path_in_src_tree(legate_parent_dir / "legate"):
            # We have an editable install, only now is it safe to consult the
            # variable. We cannot do it above because we needed to make sure we
            # weren't in fully-installed-mode
            from ..install_info import LEGATE_ARCH

            return LEGATE_ARCH

        # Must be either fully installed, or not yet configured, in any case,
        # we have no arch.
        return None

    legate_arch = get_legate_arch()
    if legate_arch is None:
        return None

    # legate_parent_dir is either <PREFIX>/lib/python<version>/site-packages or
    # $LEGATE_DIR/src/python.
    #
    # If it is the former, then we are fully installed, in which case both of
    # these functions will return None
    #
    # If the latter, then the arch dir will be up 2 and into legate_arch.
    legate_arch_dir = legate_parent_dir.parents[1] / legate_arch
    if (skbuild_dir := legate_arch_dir / "skbuild_core").exists():
        return get_legate_build_dir_from_skbuild_dir(skbuild_dir)
    return get_legate_build_dir_from_arch_build_dir(legate_arch_dir)


def get_legate_paths() -> LegatePaths:
    """Determine all the important runtime paths for Legate.

    Returns
    -------
    LegatePaths

    Notes
    -----
    This function may be called in 1 of 3 scenarios:
    1. The python libraries are not installed, and this is called
       (transitively) from e.g. test.py.
    2. The python libraries are regularly installed.
    3. The python libraries are installed 'editable' mode.
    """
    import legate

    legate_mod_dir = Path(legate.__path__[0])
    legate_mod_parent = legate_mod_dir.parent
    legate_build_dir = get_legate_build_dir(legate_mod_parent)

    def make_legate_bind_path(base: Path) -> Path:
        return base / "share" / "legate" / "libexec" / "legate-bind.sh"

    if legate_build_dir is None:
        import sysconfig

        # If legate_build_dir is None, then we are either dealing with an
        # installed version of legate or we may have been called from
        # test.py. legate_mod_dir is either
        # <PREFIX>/lib/python<version>/site-packages/legate, or
        # $LEGATE_DIR/src/python/legate.
        site_package_dir_name = Path(sysconfig.get_paths()["purelib"]).name
        if is_legate_path_in_src_tree(legate_mod_dir):
            # we are in the source repository, and legate_mod_dir =
            # src/python/legate, but have neither configured nor installed the
            # libraries. Most of these paths are meaningless, but let's at
            # least fill out the right bind_sh_path.
            bind_sh_path = make_legate_bind_path(legate_mod_dir.parents[2])
            legate_lib_path = Path("this_path_does_not_exist")
            assert not legate_lib_path.exists()
        elif legate_mod_parent.name == site_package_dir_name:
            # It's possible we are in an installed library, in which case
            # legate_mod_dir is probably
            # <PREFIX>/lib/python<version>/site-packages/legate. In this case,
            # legate-bind.sh and the libs are under
            # <PREFIX>/share/legate/libexec/legate-bind.sh and <PREFIX>/lib
            # respectively.
            prefix_dir = legate_mod_dir.parents[3]
            bind_sh_path = make_legate_bind_path(prefix_dir)
            legate_lib_path = prefix_dir / "lib"
            assert_dir_exists(legate_lib_path)
        else:
            msg = f"Unhandled legate module install location: {legate_mod_dir}"
            raise RuntimeError(msg)

        assert_file_exists(bind_sh_path)
        return LegatePaths(
            legate_dir=legate_mod_dir,
            legate_build_dir=legate_build_dir,
            bind_sh_path=bind_sh_path,
            legate_lib_path=legate_lib_path,
        )

    # If build_dir is not None, then we almost certainly have an editable
    # install, or are being called by test.py
    cmake_cache_txt = legate_build_dir / "CMakeCache.txt"

    src_dir = Path(
        read_cmake_cache_value(
            cmake_cache_txt, "legate_cpp_SOURCE_DIR:STATIC="
        )
    ).parent
    bind_sh_path = make_legate_bind_path(src_dir)

    legate_binary_dir = Path(
        read_cmake_cache_value(
            cmake_cache_txt, "legate_cpp_BINARY_DIR:STATIC="
        )
    )

    legate_lib_path = legate_binary_dir / "cpp" / "lib"

    assert_dir_exists(legate_mod_dir)
    assert_dir_exists(legate_build_dir)
    assert_file_exists(bind_sh_path)
    assert_dir_exists(legate_lib_path)
    return LegatePaths(
        legate_dir=legate_mod_dir,
        legate_build_dir=legate_build_dir,
        bind_sh_path=bind_sh_path,
        legate_lib_path=legate_lib_path,
    )


def get_legion_paths(legate_paths: LegatePaths) -> LegionPaths:
    """Determine all the important runtime paths for Legion.

    Parameters
    ----------
        legate_paths : LegatePaths
            Locations of Legate runtime paths

    Returns
    -------
        LegionPaths

    """
    # Construct and return paths needed to interact with legion, accounting
    # for multiple ways Legion and legate may be configured or installed.
    #
    # 1. Legion was found in a standard system location (/usr, $CONDA_PREFIX)
    # 2. Legion was built as a side-effect of building legate:
    #    ```
    #    CMAKE_ARGS="" python -m pip install .
    #    ```
    # 3. Legion was built in a separate directory independent of legate
    #    and the path to its build directory was given when configuring
    #    legate:
    #    ```
    #    CMAKE_ARGS="-D Legion_ROOT=/legion/build" \
    #        python -m pip install .
    #    ```
    #
    # Additionally, legate has multiple run modes:
    #
    # 1. As an installed Python module (`python -m pip install .`)
    # 2. As an "editable" install (`python -m pip install --editable .`)
    #
    # When determining locations of Legion and legate paths, prioritize
    # local builds over global installations. This allows devs to work in the
    # source tree and re-run without overwriting existing installations.

    def installed_legion_paths(legion_dir: Path) -> LegionPaths:
        legion_lib_dir = legion_dir / "lib"
        for f in legion_lib_dir.iterdir():
            legion_module = f / "site-packages"
            if legion_module.exists():
                break

        # NB: for-else clause! (executes if NO loop break)
        else:
            msg = "could not determine legion module location"
            raise RuntimeError(msg)

        legion_bin_path = legion_dir / "bin"
        legion_include_path = legion_dir / "include"

        return LegionPaths(
            legion_bin_path=legion_bin_path,
            legion_lib_path=legion_lib_dir,
            realm_defines_h=legion_include_path / "realm_defines.h",
            legion_defines_h=legion_include_path / "legion_defines.h",
            legion_spy_py=legion_bin_path / "legion_spy.py",
            legion_prof=legion_bin_path / "legion_prof",
            legion_module=legion_module,
            legion_jupyter_module=legion_module,
        )

    if (legate_build_dir := legate_paths.legate_build_dir) is None:
        legate_build_dir = get_legate_build_dir(legate_paths.legate_dir.parent)

    # If no local build dir found, assume legate installed into the python env
    if legate_build_dir is None:
        return installed_legion_paths(legate_paths.legate_dir.parents[3])

    # If a legate build dir was found, read `Legion_SOURCE_DIR` and
    # `Legion_BINARY_DIR` from in CMakeCache.txt, return paths into the source
    # and build dirs. This allows devs to quickly rebuild inplace and use the
    # most up-to-date versions without needing to install Legion and
    # legate globally.

    cmake_cache_txt = legate_build_dir / "CMakeCache.txt"

    try:
        try:
            # Test whether Legion_DIR is set. If it isn't, then we built
            # Legion as a side-effect of building legate
            read_cmake_cache_value(
                cmake_cache_txt, "Legion_DIR:PATH=Legion_DIR-NOTFOUND"
            )
        except Exception:
            # If Legion_DIR is a valid path, check whether it's a
            # Legion build dir, i.e. `-D Legion_ROOT=/legion/build`
            legion_dir = Path(
                read_cmake_cache_value(cmake_cache_txt, "Legion_DIR:PATH=")
            )
            if legion_dir.joinpath("CMakeCache.txt").exists():
                cmake_cache_txt = legion_dir / "CMakeCache.txt"
    finally:
        # Hopefully at this point we have a valid cmake_cache_txt with a
        # valid Legion_SOURCE_DIR and Legion_BINARY_DIR
        with contextlib.suppress(Exception):
            # If Legion_SOURCE_DIR and Legion_BINARY_DIR are in CMakeCache.txt,
            # return the paths to Legion in the legate build dir.
            legion_source_dir = Path(
                read_cmake_cache_value(
                    cmake_cache_txt, "Legion_SOURCE_DIR:STATIC="
                )
            )
            legion_binary_dir = Path(
                read_cmake_cache_value(
                    cmake_cache_txt, "Legion_BINARY_DIR:STATIC="
                )
            )

            legion_runtime_dir = legion_binary_dir / "runtime"
            legion_bindings_dir = legion_source_dir / "bindings"

            return LegionPaths(  # noqa: B012
                legion_bin_path=legion_binary_dir / "bin",
                legion_lib_path=legion_binary_dir / "lib",
                realm_defines_h=legion_runtime_dir / "realm_defines.h",
                legion_defines_h=legion_runtime_dir / "legion_defines.h",
                legion_spy_py=legion_source_dir / "tools" / "legion_spy.py",
                legion_prof=legion_binary_dir / "bin" / "legion_prof",
                legion_module=legion_bindings_dir / "python" / "build" / "lib",
                legion_jupyter_module=legion_source_dir / "jupyter_notebook",
            )

    # Otherwise return the installation paths.
    return installed_legion_paths(Path(sys.argv[0]).parents[1])
