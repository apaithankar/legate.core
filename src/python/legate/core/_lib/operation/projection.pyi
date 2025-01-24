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

import typing_extensions

from ..utilities.unconstructable import Unconstructable

class SymbolicExpr(Unconstructable):
    @property
    def dim(self) -> int: ...
    @property
    def weight(self) -> int: ...
    @property
    def offset(self) -> int: ...
    def is_identity(self, dim: int) -> bool: ...
    def __eq__(self, other: object) -> bool: ...
    def __mul(self, other: int) -> SymbolicExpr: ...
    def __add__(self, other: int) -> SymbolicExpr: ...

def dimension(dim: int) -> SymbolicExpr: ...
def constant(value: int) -> SymbolicExpr: ...

SymbolicPoint: typing_extensions.TypeAlias = tuple[SymbolicExpr, ...]
