# SPDX-FileCopyrightText: Copyright (c) 2021-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from collections.abc import Sequence
from typing import Callable, overload

from ..._lib.partitioning.constraint import ConstraintProxy
from .py_task import PyTask
from .type import UserFunction, VariantList

@overload
def task(func: UserFunction) -> PyTask: ...
@overload
def task(
    *,
    variants: VariantList = ...,
    constraints: Sequence[ConstraintProxy] | None = None,
    throws_exception: bool = False,
    has_side_effect: bool = False,
    register: bool = True,
) -> Callable[[UserFunction], PyTask]: ...
