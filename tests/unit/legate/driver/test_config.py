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

import os
from pathlib import Path
from unittest.mock import call

import pytest
from pytest_mock import MockerFixture

import legate.driver.config as m
import legate.driver.defaults as defaults
from legate.util import colors
from legate.util.colors import scrub
from legate.util.types import DataclassMixin

from ...util import Capsys, powerset

DEFAULTS_ENV_VARS = (
    "LEGATE_EAGER_ALLOC_PERCENTAGE",
    "LEGATE_FBMEM",
    "LEGATE_NUMAMEM",
    "LEGATE_OMP_PROCS",
    "LEGATE_OMP_THREADS",
    "LEGATE_REGMEM",
    "LEGATE_SYSMEM",
    "LEGATE_UTILITY_CORES",
    "LEGATE_ZCMEM",
)


class TestMultiNode:
    def test_fields(self) -> None:
        assert set(m.MultiNode.__dataclass_fields__) == {
            "nodes",
            "ranks_per_node",
            "launcher",
            "launcher_extra",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.MultiNode, DataclassMixin)

    @pytest.mark.parametrize(
        "extra",
        (["a"], ["a", "b c"], ["a", "b c", "d e"], ["a", "b c", "d e", "f"]),
    )
    def test_launcher_extra_fixup_basic(self, extra: list[str]) -> None:
        mn = m.MultiNode(
            nodes=1,
            ranks_per_node=1,
            launcher="mpirun",
            launcher_extra=extra,
        )
        assert mn.launcher_extra == sum((x.split() for x in extra), [])

    def test_launcher_extra_fixup_complex(self) -> None:
        mn = m.MultiNode(
            nodes=1,
            ranks_per_node=1,
            launcher="mpirun",
            launcher_extra=[
                "-H g0002,g0002 -X SOMEENV --fork",
                "-bind-to none",
            ],
        )
        assert mn.launcher_extra == [
            "-H",
            "g0002,g0002",
            "-X",
            "SOMEENV",
            "--fork",
            "-bind-to",
            "none",
        ]

    def test_launcher_extra_fixup_quoted(self) -> None:
        mn = m.MultiNode(
            nodes=1,
            ranks_per_node=1,
            launcher="mpirun",
            launcher_extra=[
                "-f 'some path with spaces/foo.txt'",
            ],
        )
        assert mn.launcher_extra == [
            "-f",
            "some path with spaces/foo.txt",
        ]


class TestBinding:
    def test_fields(self) -> None:
        assert set(m.Binding.__dataclass_fields__) == {
            "cpu_bind",
            "mem_bind",
            "gpu_bind",
            "nic_bind",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Binding, DataclassMixin)


class TestCore:
    def test_fields(self) -> None:
        assert set(m.Core.__dataclass_fields__) == {
            "cpus",
            "gpus",
            "openmp",
            "ompthreads",
            "utility",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Core, DataclassMixin)


class TestMemory:
    def test_fields(self) -> None:
        assert set(m.Memory.__dataclass_fields__) == {
            "sysmem",
            "numamem",
            "fbmem",
            "zcmem",
            "regmem",
            "eager_alloc",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Memory, DataclassMixin)


class TestProfiling:
    def test_fields(self) -> None:
        assert set(m.Profiling.__dataclass_fields__) == {
            "profile",
            "cprofile",
            "nvprof",
            "nsys",
            "nsys_targets",
            "nsys_extra",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Profiling, DataclassMixin)

    @pytest.mark.parametrize(
        "extra",
        (["a"], ["a", "b c"], ["a", "b c", "d e"], ["a", "b c", "d e", "f"]),
    )
    def test_nsys_extra_fixup_basic(self, extra: list[str]) -> None:
        p = m.Profiling(
            profile=True,
            cprofile=True,
            nvprof=True,
            nsys=True,
            nsys_targets="foo,bar",
            nsys_extra=extra,
        )
        assert p.nsys_extra == sum((x.split() for x in extra), [])

    def test_nsys_extra_fixup_complex(self) -> None:
        p = m.Profiling(
            profile=True,
            cprofile=True,
            nvprof=True,
            nsys=True,
            nsys_targets="foo,bar",
            nsys_extra=[
                "-H g0002,g0002 -X SOMEENV --fork",
                "-bind-to none",
            ],
        )
        assert p.nsys_extra == [
            "-H",
            "g0002,g0002",
            "-X",
            "SOMEENV",
            "--fork",
            "-bind-to",
            "none",
        ]

    def test_nsys_extra_fixup_quoted(self) -> None:
        p = m.Profiling(
            profile=True,
            cprofile=True,
            nvprof=True,
            nsys=True,
            nsys_targets="foo,bar",
            nsys_extra=[
                "-f 'some path with spaces/foo.txt'",
            ],
        )
        assert p.nsys_extra == [
            "-f",
            "some path with spaces/foo.txt",
        ]


class TestLogging:
    def test_fields(self) -> None:
        assert set(m.Logging.__dataclass_fields__) == {
            "user_logging_levels",
            "logdir",
            "log_to_file",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Logging, DataclassMixin)


class TestDebugging:
    def test_fields(self) -> None:
        assert set(m.Debugging.__dataclass_fields__) == {
            "gdb",
            "cuda_gdb",
            "memcheck",
            "valgrind",
            "freeze_on_error",
            "gasnet_trace",
            "spy",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Debugging, DataclassMixin)


class TestInfo:
    def test_fields(self) -> None:
        assert set(m.Info.__dataclass_fields__) == {
            "verbose",
            "bind_detail",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Info, DataclassMixin)


class TestOther:
    def test_fields(self) -> None:
        assert set(m.Other.__dataclass_fields__) == {
            "timing",
            "wrapper",
            "wrapper_inner",
            "module",
            "dry_run",
        }

    def test_mixin(self) -> None:
        assert issubclass(m.Other, DataclassMixin)


class TestConfig:
    def test_default_init(self) -> None:
        # Note this test does not clear the environment. Default values from
        # the defaults module can depend on the environment, but what matters
        # is that the generated config matches those values, whatever they are.

        c = m.Config(["legate"])

        assert colors.ENABLED is False

        assert c.multi_node == m.MultiNode(
            nodes=defaults.LEGATE_NODES,
            ranks_per_node=defaults.LEGATE_RANKS_PER_NODE,
            launcher="none",
            launcher_extra=[],
        )
        assert c.binding == m.Binding(
            cpu_bind=None,
            mem_bind=None,
            gpu_bind=None,
            nic_bind=None,
        )
        assert c.core == m.Core(
            cpus=4,
            gpus=0,
            openmp=defaults.LEGATE_OMP_PROCS,
            ompthreads=defaults.LEGATE_OMP_THREADS,
            utility=defaults.LEGATE_UTILITY_CORES,
        )

        c.memory == m.Memory(
            sysmem=defaults.LEGATE_SYSMEM,
            numamem=defaults.LEGATE_NUMAMEM,
            fbmem=defaults.LEGATE_FBMEM,
            zcmem=defaults.LEGATE_ZCMEM,
            regmem=defaults.LEGATE_REGMEM,
            eager_alloc=defaults.LEGATE_EAGER_ALLOC_PERCENTAGE,
        )

        c.profiling == m.Profiling(
            profile=False,
            cprofile=False,
            nvprof=False,
            nsys=False,
            nsys_targets="",
            nsys_extra=[],
        )

        assert c.logging == m.Logging(
            user_logging_levels=None,
            logdir=Path(os.getcwd()),
            log_to_file=False,
        )

        assert c.debugging == m.Debugging(
            gdb=False,
            cuda_gdb=False,
            memcheck=False,
            valgrind=False,
            freeze_on_error=False,
            gasnet_trace=False,
            spy=False,
        )

        assert c.info == m.Info(verbose=False, bind_detail=False)

        assert c.other == m.Other(
            timing=False,
            wrapper=[],
            wrapper_inner=[],
            module=None,
            dry_run=False,
        )

    def test_color_arg(self) -> None:
        m.Config(["legate", "--color"])

        assert colors.ENABLED is True

    def test_arg_conversions(self, mocker: MockerFixture) -> None:
        # This is kind of a dumb short-cut test, but if we believe that
        # object_to_dataclass works as advertised, then this test ensures that
        # it is being used for all the sub-configs that it should be used for

        spy = mocker.spy(m, "object_to_dataclass")

        c = m.Config(["legate"])

        assert spy.call_count == 9
        spy.assert_has_calls(
            [
                call(c._args, m.MultiNode),
                call(c._args, m.Binding),
                call(c._args, m.Core),
                call(c._args, m.Memory),
                call(c._args, m.Profiling),
                call(c._args, m.Logging),
                call(c._args, m.Debugging),
                call(c._args, m.Info),
                call(c._args, m.Other),
            ]
        )

    def test_log_to_file_fixup(self, capsys: Capsys) -> None:
        c = m.Config(["legate", "--logging", "foo", "--spy"])

        assert c.logging.log_to_file

        out, _ = capsys.readouterr()
        assert scrub(out).strip() == (
            "WARNING: Logging output is being redirected to a file in "
            f"directory {c.logging.logdir}"
        )

    # maybe this is overkill but this is literally the point where the user's
    # own script makes contact with legate, so let's make extra sure that that
    # ingest succeeds over a very wide range of command line combinations (one
    # option from most sub-configs)
    @pytest.mark.parametrize(
        "args",
        powerset(
            (
                "--spy",
                "--gdb",
                "--profile",
                "--cprofile",
            )
        ),
    )
    def test_user_opts(self, args: tuple[str, ...]) -> None:
        c = m.Config(["legate"] + list(args) + ["foo.py", "-a", "1"])

        assert c.user_opts == ("-a", "1")
        assert c.user_program == "foo.py"

    USER_OPTS: tuple[list[str], ...] = (
        [],
        ["-a"],
        ["-a", "-b", "1", "--long"],
    )

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_exec_run_mode_with_prog_with_module(
        self, opts: list[str]
    ) -> None:
        with pytest.raises(RuntimeError):
            m.Config(
                ["legate", "--run-mode", "exec", "--module", "mod", "prog"]
                + opts
            )

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_exec_run_mode_with_prog_no_module(self, opts: list[str]) -> None:
        c = m.Config(["legate", "--run-mode", "exec", "prog"] + opts)

        assert c.user_opts == tuple(opts)
        assert c.run_mode == "exec"
        assert not c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_python_run_mode_with_prog_with_module(
        self, opts: list[str]
    ) -> None:
        c = m.Config(
            ["legate", "--run-mode", "python", "--module", "mod", "prog"]
            + opts
        )

        assert c.user_opts == tuple(opts)
        assert c.run_mode == "python"
        assert not c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_python_run_mode_with_prog_no_module(
        self, opts: list[str]
    ) -> None:
        c = m.Config(["legate", "--run-mode", "python", "prog"] + opts)

        assert c.user_opts == tuple(opts)
        assert c.run_mode == "python"
        assert not c.console

    def test_python_run_mode_no_prog_with_module(self) -> None:
        c = m.Config(["legate", "--run-mode", "python", "--module", "mod"])

        assert c.user_opts == ()
        assert c.run_mode == "python"
        assert c.console

    def test_python_run_mode_no_prog_no_module(self) -> None:
        c = m.Config(["legate", "--run-mode", "python"])

        assert c.user_opts == ()
        assert c.run_mode == "python"
        assert c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_default_run_mode_with_script_with_module(
        self, opts: list[str]
    ) -> None:
        c = m.Config(
            [
                "legate",
                "--gpus",
                "2",
                "--module",
                "mod",
                "script.py",
            ]
            + opts
        )

        assert c.user_opts == tuple(opts)
        assert c.user_program == "script.py"
        assert c.run_mode == "python"
        assert not c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_default_run_mode_with_script_no_module(
        self, opts: list[str]
    ) -> None:
        c = m.Config(["legate", "--gpus", "2", "script.py"] + opts)

        assert c.user_opts == tuple(opts)
        assert c.user_program == "script.py"
        assert c.run_mode == "python"
        assert not c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_default_run_mode_with_prog_with_modue(
        self, opts: list[str]
    ) -> None:
        c = m.Config(
            ["legate", "--gpus", "2", "--module", "mod", "prog"] + opts
        )

        assert c.user_opts == tuple(opts)
        assert c.user_program == "prog"
        assert c.run_mode == "python"
        assert not c.console

    @pytest.mark.parametrize("opts", USER_OPTS)
    def test_default_run_mode_with_prog_no_modue(
        self, opts: list[str]
    ) -> None:
        c = m.Config(["legate", "--gpus", "2", "prog"] + opts)

        assert c.user_opts == tuple(opts)
        assert c.user_program == "prog"
        assert c.run_mode == "exec"
        assert not c.console

    def test_default_run_mode_no_prog_with_module(self) -> None:
        c = m.Config(["legate", "--module", "mod"])

        assert c.user_opts == ()
        assert c.run_mode == "python"
        assert c.console

    def test_default_run_mode_no_prog_no_module(self) -> None:
        c = m.Config(["legate"])

        assert c.user_opts == ()
        assert c.run_mode == "python"
        assert c.console

    def test_exec_run_mode_no_prog_with_module(self) -> None:
        with pytest.raises(RuntimeError):
            m.Config(["legate", "--run-mode", "exec", "--module", "mod"])

    def test_exec_run_mode_no_prog_no_module(self) -> None:
        with pytest.raises(RuntimeError):
            m.Config(["legate", "--run-mode", "exec"])
