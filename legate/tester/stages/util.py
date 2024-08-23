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

from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import TypeAlias

from ...util.ui import dim, failed, passed, shell, skipped, timeout, yellow
from ..config import Config
from ..logger import LOG
from ..test_system import ProcessResult

UNPIN_ENV = {"REALM_SYNTHETIC_CORE_MAP": ""}

# Raise min chunk sizes for deferred codepaths to force eager execution
EAGER_ENV = {
    "CUNUMERIC_FORCE_THUNK": "eager",
    "CUNUMERIC_MIN_CPU_CHUNK": "2000000000",
    "CUNUMERIC_MIN_OMP_CHUNK": "2000000000",
    "CUNUMERIC_MIN_GPU_CHUNK": "2000000000",
}


RankShard: TypeAlias = tuple[int, ...]


@dataclass(frozen=True)
class Shard:
    """Specify how resources should be allotted for each test process"""

    #: A list of shards for each rank
    ranks: list[RankShard]

    def __str__(self) -> str:
        return "/".join(",".join(str(r) for r in rank) for rank in self.ranks)


@dataclass(frozen=True)
class StageSpec:
    """Specify the operation of a test run"""

    #: The number of worker processes to start for running tests
    workers: int

    # A list of (cpu or gpu) shardings to draw on for each test
    shards: list[Shard]


@dataclass(frozen=True)
class StageResult:
    """Collect results from all tests in a TestStage."""

    #: Individual test process results including return code and stdout.
    procs: list[ProcessResult]

    #: Cumulative execution time for all tests in a stage.
    time: timedelta

    @property
    def total(self) -> int:
        """The total number of tests run in this stage."""
        return len(self.procs)

    @property
    def passed(self) -> int:
        """The number of tests in this stage that passed."""
        return sum(p.passed for p in self.procs)


def adjust_workers(
    workers: int, requested_workers: int | None, *, detail: str | None = None
) -> int:
    """Adjust computed workers according to command line requested workers.

    The final number of workers will only be adjusted down by this function.

    Parameters
    ----------
    workers: int
        The computed number of workers to use

    requested_workers: int | None, optional
        Requested number of workers from the user, if supplied (default: None)

    detail: str | None, optional
        Additional information to provide in case the adjusted number of
        workers is zero (default: None)

    Returns
    -------
    int
        The number of workers to actually use

    """
    if requested_workers is not None and requested_workers < 0:
        raise ValueError("requested workers must be non-negative")

    if requested_workers == 0:
        raise RuntimeError("requested workers must not be zero")

    if requested_workers is not None:
        if requested_workers > workers:
            raise RuntimeError(
                f"Requested workers ({requested_workers}) is greater than "
                f"computed workers ({workers})"
            )
        workers = requested_workers

    if workers == 0:
        msg = "Current configuration results in zero workers"
        if detail:
            msg += f" [details: {detail}]"
        raise RuntimeError(msg)

    return workers


def format_duration(start: datetime, end: datetime) -> str:
    r"""Format a duration from START to END for display.

    Parameters
    ----------
    start : datetime
        The start of the duration
    end : datetime
        The end of the duration

    Returns
    -------
    str
        The formatted duration

    Raises
    ------
    ValueError
        If the duration is invalid, such as when end comes before start.
    """
    if end < start:
        raise ValueError(f"End ({end}) happens before start ({start})")

    duration = (end - start).total_seconds()
    time = f"{duration:0.2f}s"
    start_str = start.strftime("%H:%M:%S.%f")[:-4]
    end_str = end.strftime("%H:%M:%S.%f")[:-4]
    return f" {yellow(time)} " + dim(f"{{{start_str}, {end_str}}}")


def log_proc(
    name: str, proc: ProcessResult, config: Config, *, verbose: bool
) -> None:
    """Log a process result according to the current configuration"""
    if config.info.debug or config.dry_run:
        LOG(shell(proc.invocation))

    if proc.time is None or proc.start is None or proc.end is None:
        duration = ""
    else:
        assert proc.end - proc.start == proc.time
        duration = format_duration(proc.start, proc.end)

    msg = f"({name}){duration} {proc.test_display}"
    details = proc.output.split("\n") if verbose else None
    if proc.skipped:
        LOG(skipped(msg))
    elif proc.timeout:
        LOG(timeout(msg, details=details))
    elif proc.returncode == 0:
        LOG(passed(msg, details=details))
    else:
        LOG(failed(msg, details=details, exit_code=proc.returncode))
