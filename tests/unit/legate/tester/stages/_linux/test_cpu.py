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

"""Consolidate test configuration from command-line and environment.

"""
from __future__ import annotations

import pytest

from legate.tester.config import Config
from legate.tester.stages._linux import cpu as m
from legate.tester.stages.util import UNPIN_ENV, Shard

from .. import FakeSystem

unpin_and_test = dict(UNPIN_ENV)


def test_default() -> None:
    c = Config([])
    s = FakeSystem(cpus=12)
    stage = m.CPU(c, s)
    assert stage.kind == "cpus"
    assert stage.args == []
    assert stage.env(c, s) == unpin_and_test
    assert stage.spec.workers > 0

    shard = (1, 2, 3)
    assert "--cpu-bind" in stage.shard_args(Shard([shard]), c)


def test_cpu_pin_strict() -> None:
    c = Config(["test.py", "--cpu-pin", "strict"])
    s = FakeSystem(cpus=12)
    stage = m.CPU(c, s)
    assert stage.kind == "cpus"
    assert stage.args == []
    assert stage.env(c, s) == {}
    assert stage.spec.workers > 0

    shard = (1, 2, 3)
    assert "--cpu-bind" in stage.shard_args(Shard([shard]), c)


def test_cpu_pin_strict_zero_computed_workers() -> None:
    c = Config(["test.py", "--cpu-pin", "strict", "--cpus", "16"])
    s = FakeSystem(cpus=12)
    with pytest.raises(RuntimeError, match="not enough"):
        m.CPU(c, s)


def test_cpu_pin_nonstrict_zero_computed_workers() -> None:
    c = Config(["test.py", "--cpus", "16"])
    s = FakeSystem(cpus=12)
    stage = m.CPU(c, s)
    assert stage.kind == "cpus"
    assert stage.args == []
    assert stage.env(c, s) == unpin_and_test
    assert stage.spec.workers == 1

    shard = tuple(range(12))
    assert "--cpu-bind" in stage.shard_args(Shard([shard]), c)


def test_cpu_pin_none() -> None:
    c = Config(["test.py", "--cpu-pin", "none"])
    s = FakeSystem(cpus=12)
    stage = m.CPU(c, s)
    assert stage.kind == "cpus"
    assert stage.args == []
    assert stage.env(c, s) == unpin_and_test
    assert stage.spec.workers > 0

    shard = (1, 2, 3)
    assert "--cpu-bind" not in stage.shard_args(Shard([shard]), c)


class TestSingleRank:
    @pytest.mark.parametrize(
        "shard,expected", [[(2,), "2"], [(1, 2, 3), "1,2,3"]]
    )
    def test_shard_args(self, shard: tuple[int, ...], expected: str) -> None:
        c = Config(["test.py", "--sysmem", "2000"])
        s = FakeSystem()
        stage = m.CPU(c, s)
        result = stage.shard_args(Shard([shard]), c)
        assert result == [
            "--cpus",
            f"{c.core.cpus}",
            "--sysmem",
            str(c.memory.sysmem),
            "--cpu-bind",
            expected,
        ]

    def test_spec_with_cpus_1(self) -> None:
        c = Config(["test.py", "--cpus", "1"])
        s = FakeSystem()
        stage = m.CPU(c, s)
        assert stage.spec.workers == 3
        assert stage.spec.shards == [
            Shard([(0, 1)]),
            Shard([(2, 3)]),
            Shard([(4, 5)]),
        ]

    def test_spec_with_cpus_2(self) -> None:
        c = Config(["test.py", "--cpus", "2"])
        s = FakeSystem()
        stage = m.CPU(c, s)
        assert stage.spec.workers == 2
        assert stage.spec.shards == [
            Shard([(0, 1, 2)]),
            Shard([(3, 4, 5)]),
        ]

    def test_spec_with_utility(self) -> None:
        c = Config(["test.py", "--cpus", "1", "--utility", "2"])
        s = FakeSystem()
        stage = m.CPU(c, s)
        assert stage.spec.workers == 2
        assert stage.spec.shards == [
            Shard([(0, 1, 2)]),
            Shard([(3, 4, 5)]),
        ]

    def test_spec_with_requested_workers(
        self,
    ) -> None:
        c = Config(["test.py", "--cpus", "1", "-j", "2"])
        s = FakeSystem()
        stage = m.CPU(c, s)
        assert stage.spec.workers == 2
        assert stage.spec.shards == [
            Shard([(0, 1)]),
            Shard([(2, 3)]),
        ]

    def test_spec_with_requested_workers_zero(self) -> None:
        s = FakeSystem()
        c = Config(["test.py", "-j", "0"])
        assert c.execution.workers == 0
        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    def test_spec_with_requested_workers_bad(self) -> None:
        s = FakeSystem()
        c = Config(["test.py", "-j", f"{len(s.cpus) + 1}"])
        requested_workers = c.execution.workers
        assert requested_workers is not None
        assert requested_workers > len(s.cpus)
        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    def test_spec_with_verbose(self) -> None:
        args = ["test.py", "--cpus", "2"]
        c = Config(args)
        cv = Config(args + ["--verbose"])
        s = FakeSystem()

        spec, vspec = m.CPU(c, s).spec, m.CPU(cv, s).spec
        assert vspec == spec

    @pytest.mark.parametrize("cpus", (4, 5, 10, 20))
    def test_oversubscription_with_pin(self, cpus: int) -> None:
        args = ["test.py", "--cpus", str(cpus), "--cpu-pin", "strict"]
        c = Config(args)
        s = FakeSystem(cpus=4)

        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    @pytest.mark.parametrize("cpus", (4, 5, 10, 20))
    def test_oversubscription_no_pin(self, cpus: int) -> None:
        args = ["test.py", "--cpus", str(cpus), "--cpu-pin", "none"]
        c = Config(args)
        s = FakeSystem(cpus=4)

        with pytest.warns():
            stage = m.CPU(c, s)

        assert stage.spec.workers == 1
        assert stage.spec.shards == [
            Shard([(0, 1, 2, 3)]),
        ]


class TestMultiRank:
    def test_shard_args(self) -> None:
        c = Config(
            [
                "test.py",
                "--cpus",
                "2",
                "--ranks-per-node",
                "2",
                "--sysmem",
                "2000",
                # any launcher will do
                "--launcher",
                "srun",
            ]
        )
        s = FakeSystem(cpus=12)
        stage = m.CPU(c, s)
        result = stage.shard_args(Shard([(0, 1), (2, 3)]), c)
        assert result == [
            "--cpus",
            f"{c.core.cpus}",
            "--sysmem",
            str(c.memory.sysmem),
            "--cpu-bind",
            "0,1/2,3",
            "--launcher",
            "srun",
            "--ranks-per-node",
            "2",
        ]

    def test_spec_with_cpus_1(self) -> None:
        c = Config(
            [
                "test.py",
                "--cpus",
                "1",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ]
        )
        s = FakeSystem(cpus=12)
        stage = m.CPU(c, s)
        assert stage.spec.workers == 3
        assert stage.spec.shards == [
            Shard([(0, 1), (2, 3)]),
            Shard([(4, 5), (6, 7)]),
            Shard([(8, 9), (10, 11)]),
        ]

    def test_spec_with_cpus_2(self) -> None:
        c = Config(
            [
                "test.py",
                "--cpus",
                "2",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ]
        )
        s = FakeSystem(cpus=12)
        stage = m.CPU(c, s)
        assert stage.spec.workers == 2
        assert stage.spec.shards == [
            Shard([(0, 1, 2), (3, 4, 5)]),
            Shard([(6, 7, 8), (9, 10, 11)]),
        ]

    def test_spec_with_utility_2(self) -> None:
        c = Config(
            [
                "test.py",
                "--cpus",
                "1",
                "--utility",
                "2",
                "--ranks-per-node",
                "2",
                # any launcher will do
                "--launcher",
                "srun",
            ]
        )
        s = FakeSystem(cpus=12)
        stage = m.CPU(c, s)
        assert stage.spec.workers == 2
        assert stage.spec.shards == [
            Shard([(0, 1, 2), (3, 4, 5)]),
            Shard([(6, 7, 8), (9, 10, 11)]),
        ]

    def test_spec_with_requested_workers(self) -> None:
        c = Config(
            [
                "test.py",
                "--cpus",
                "1",
                "-j",
                "1",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ]
        )
        s = FakeSystem(cpus=12)
        stage = m.CPU(c, s)
        assert stage.spec.workers == 1
        assert stage.spec.shards == [
            Shard([(0, 1), (2, 3)]),
        ]

    def test_spec_with_requested_workers_zero(self) -> None:
        s = FakeSystem(cpus=12)
        c = Config(
            [
                "test.py",
                "-j",
                "0",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ]
        )
        assert c.execution.workers == 0
        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    def test_spec_with_requested_workers_bad(self) -> None:
        s = FakeSystem(cpus=12)
        c = Config(
            [
                "test.py",
                "-j",
                f"{len(s.cpus) + 1}",
                "--ranks-per-node",
                "2",
                "--launcher",
                "srun",
            ]
        )
        requested_workers = c.execution.workers
        assert requested_workers is not None
        assert requested_workers > len(s.cpus)
        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    @pytest.mark.parametrize("cpus", (2, 3, 10, 20))
    def test_oversubscription_with_pin(self, cpus: int) -> None:
        args = [
            "test.py",
            "--cpus",
            str(cpus),
            "--ranks-per-node",
            "2",
            "--cpu-pin",
            "strict",
            # any launcher will do
            "--launcher",
            "srun",
        ]
        c = Config(args)
        s = FakeSystem(cpus=4)

        with pytest.raises(RuntimeError):
            m.CPU(c, s)

    @pytest.mark.parametrize("cpus", (2, 3, 10, 20))
    def test_oversubscription_no_pin(self, cpus: int) -> None:
        args = [
            "test.py",
            "--cpus",
            str(cpus),
            "--ranks-per-node",
            "2",
            "--cpu-pin",
            "none",
            # any launcher will do
            "--launcher",
            "srun",
        ]
        c = Config(args)
        s = FakeSystem(cpus=4)

        with pytest.warns():
            stage = m.CPU(c, s)

        assert stage.spec.workers == 1
        assert stage.spec.shards == [
            Shard([(0, 1, 2, 3)]),
        ]
