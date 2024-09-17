#!/bin/bash

# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

legate_root=$(python -c 'import legate.install_info as i; from pathlib import Path; print(Path(i.libpath).parent.resolve())')
echo "Using Legate at ${legate_root}"
cmake -S . -B build -D legate_core_ROOT="${legate_root}"
cmake --build build --parallel 4
python -m pip install -e .
