# SPDX-FileCopyrightText: Copyright (c) 2025-2025 NVIDIA CORPORATION & AFFILIATES.
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

import re
from typing import TYPE_CHECKING, Final

if TYPE_CHECKING:
    from pathlib import Path

    from .context import Context

LEGATE_VERSION_RE: Final = re.compile(
    r"(set\s+legate_version\s+=\s+)'\d+\.\d+\.\d+'"
)


def _do_bump_legate_profiler_version(ctx: Context, meta_yaml: Path) -> None:
    ctx.vprint(f"Opening {meta_yaml}")
    assert meta_yaml.is_file()
    lines = meta_yaml.read_text()
    next_full_ver = ctx.to_full_version(ctx.next_version, extra_zeros=True)
    new_lines = LEGATE_VERSION_RE.sub(rf"\1'{next_full_ver}'", lines)
    if new_lines == lines:
        ctx.vprint("Legate profiler version already bumped")
        return

    if not ctx.dry_run:
        meta_yaml.write_text(new_lines)
    ctx.vprint(f"Updated {meta_yaml}")


def bump_legate_profiler_version(ctx: Context) -> None:
    profiler_dir = ctx.legate_dir / "conda" / "legate_profiler"
    assert profiler_dir.is_dir()

    for base_dir in (profiler_dir, profiler_dir / "dummy_legate"):
        _do_bump_legate_profiler_version(ctx, base_dir / "meta.yaml")
