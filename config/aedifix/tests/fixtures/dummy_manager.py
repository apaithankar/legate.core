# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.B
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

from collections.abc import Callable, Sequence
from typing import Any, ParamSpec, TypeVar

import pytest

from ...manager import ConfigurationManager
from .dummy_main_module import DummyMainModule

_T = TypeVar("_T")
_P = ParamSpec("_P")


class DummyManager(ConfigurationManager):
    def log(self, *args: Any, **kwargs: Any) -> None:
        pass

    def log_divider(self, *args: Any, **kwargs: Any) -> None:
        pass

    def log_boxed(self, *args: Any, **kwargs: Any) -> None:
        pass

    def log_warning(self, *args: Any, **kwargs: Any) -> None:
        pass

    def log_execute_command(
        self, cmd: Sequence[_T], live: bool = False
    ) -> Any:
        pass

    def log_execute_func(  # type: ignore[override]
        self, func: Callable[_P, _T], *args: _P.args, **kwargs: _P.kwargs
    ) -> _T:
        return func(*args, **kwargs)


@pytest.fixture
def manager() -> DummyManager:
    return DummyManager(tuple(), DummyMainModule)
