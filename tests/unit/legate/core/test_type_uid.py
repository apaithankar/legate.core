# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES.
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

import pytest

import legate.core.types as ty

_PRIMITIVES = [
    ty.bool_,
    ty.int8,
    ty.int16,
    ty.int32,
    ty.int64,
    ty.uint8,
    ty.uint16,
    ty.uint32,
    ty.uint64,
    ty.float16,
    ty.float32,
    ty.float64,
    ty.complex64,
    ty.complex128,
]

_PRIMITIVES_UIDS = {type.uid for type in _PRIMITIVES}


class TestFixedArrayType:
    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    @pytest.mark.parametrize("size", [1, 10, 100, 255])
    def test_uid(self, elem_type: ty.Type, size: int) -> None:
        arr_type = ty.array_type(elem_type, size)
        assert arr_type.uid & 0x00FF == elem_type.code
        assert arr_type.uid >> 8 == size

        assert arr_type.uid != ty.string_type.uid
        assert arr_type.uid not in _PRIMITIVES_UIDS

    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_same_type(self, elem_type: ty.Type) -> None:
        type1 = ty.array_type(elem_type, 1)
        type2 = ty.array_type(elem_type, 1)

        assert type1.uid == type2.uid

    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_different_types(self, elem_type: ty.Type) -> None:
        type1 = ty.array_type(elem_type, 1)
        type2 = ty.array_type(elem_type, 2)

        assert type1.uid != type2.uid

    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_big(self, elem_type: ty.Type) -> None:
        arr_type = ty.array_type(elem_type, 256)

        assert arr_type.uid >= 0x10000

        assert arr_type.uid != ty.string_type.uid
        assert arr_type.uid not in _PRIMITIVES_UIDS

    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_array_of_array_types(self, elem_type: ty.Type) -> None:
        type1 = ty.array_type(ty.array_type(elem_type, 1), 1)
        type2 = ty.array_type(ty.array_type(elem_type, 1), 1)

        assert type1.uid != type2.uid

    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_array_of_struct_types(self, elem_type: ty.Type) -> None:
        type1 = ty.array_type(ty.struct_type([elem_type]), 1)
        type2 = ty.array_type(ty.struct_type([elem_type]), 1)

        assert type1.uid != type2.uid


class TestStructType:
    @pytest.mark.parametrize("elem_type", _PRIMITIVES)
    def test_create(self, elem_type: ty.Type) -> None:
        type1 = ty.struct_type([elem_type])
        type2 = ty.struct_type([elem_type])

        assert type1.uid != type2.uid
        assert type1.uid >= 0x10000
        assert type2.uid >= 0x10000

        assert type1.uid != ty.string_type.uid
        assert type2.uid != ty.string_type.uid
        assert type1.uid not in _PRIMITIVES_UIDS
        assert type2.uid not in _PRIMITIVES_UIDS


if __name__ == "__main__":
    import sys

    sys.exit(pytest.main(sys.argv))
