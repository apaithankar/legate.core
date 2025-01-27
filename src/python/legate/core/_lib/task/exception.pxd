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

from libc.stdint cimport int32_t
from libcpp.string cimport string as std_string


cdef extern from "legate/task/exception.h" namespace "legate" nogil:
    cdef cppclass _TaskException "legate::TaskException":
        _TaskException(int32_t, std_string) except+
        _TaskException(std_string) except+
        const char* what() except+
        int32_t index() except+
        std_string error_message() except+
