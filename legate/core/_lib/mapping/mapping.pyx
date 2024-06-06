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

# Note import, not cimport. We want the Python version of the enum
from ..legate_c import legate_core_variant_t


cdef dict[TaskTarget, legate_core_variant_t] TASK_TARGET_TO_VARIANT_KIND = {
    target: getattr(legate_core_variant_t, f"_LEGATE_{target.name}_VARIANT")
    for target in TaskTarget
}
