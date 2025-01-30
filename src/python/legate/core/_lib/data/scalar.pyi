# SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES.
#                         All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

from typing import Any

from ..type.types import Type

class Scalar:
    def __init__(self, value: Any, dtype: Type | None = None): ...
    def value(self) -> Any: ...
    @property
    def type(self) -> Type: ...
    @property
    def ptr(self) -> int: ...
    @property
    def raw_handle(self) -> int: ...
    @staticmethod
    def null() -> Scalar: ...
    @property
    def __array_interface__(self) -> dict[str, Any]: ...
