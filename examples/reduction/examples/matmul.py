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


import argparse

import cunumeric as np
from reduction import matmul, multiply, sum_over_axis

import legate.core.types as ty
from legate.core import get_legate_runtime


def test(m: int, n: int, k: int, print_stores: bool, matmul_only: bool):
    # Generate inputs using cuNumeric
    rhs1 = get_legate_runtime().create_store(ty.int64, (m, k))
    rhs2 = get_legate_runtime().create_store(ty.int64, (k, n))
    np.asarray(rhs1)[:] = np.arange(m * k).reshape(m, k)
    np.asarray(rhs2)[:] = np.arange(k * n).reshape(k, n)

    if print_stores:
        print(np.asarray(rhs1))
        print(np.asarray(rhs2))

    if not matmul_only:
        # Implement matrix multiplication using sum_over_axis
        rhs1_promoted = rhs1.promote(2, n)
        rhs2_promoted = rhs2.promote(0, m)

        tmp = multiply(rhs1_promoted, rhs2_promoted)
        if print_stores:
            print(np.asarray(tmp))

        result = sum_over_axis(tmp, 1)
        if print_stores:
            print(np.asarray(result))

    result = matmul(rhs1, rhs2)
    if print_stores:
        print(np.asarray(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-m",
        type=int,
        default=3,
        dest="m",
        help="Extent of the first exterior dimension",
    )
    parser.add_argument(
        "-n",
        type=int,
        default=5,
        dest="n",
        help="Extent of the second exterior dimension",
    )
    parser.add_argument(
        "-k",
        type=int,
        default=4,
        dest="k",
        help="Extent of the contracting dimension",
    )
    parser.add_argument(
        "--print-stores",
        default=False,
        dest="print_stores",
        action="store_true",
        help="Print stores",
    )
    parser.add_argument(
        "--matmul-only",
        default=False,
        dest="matmul_only",
        action="store_true",
        help="Only call matmul",
    )
    args, _ = parser.parse_known_args()

    test(args.m, args.n, args.k, args.print_stores, args.matmul_only)
