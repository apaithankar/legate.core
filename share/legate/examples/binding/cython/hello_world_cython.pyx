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
# distutils: language=c++
# cython: language_level=3

cdef extern from "legate.h" namespace "legate":
    cdef cppclass _Library "legate::Library":
        pass

    cdef enum class LocalTaskID:
        pass

cdef extern from "hello_world.h" namespace "hello_world::HelloWorld" nogil:
    cdef LocalTaskID _TASK_ID "hello_world::HelloWorld::TASK_ID"

cdef extern from "hello_world.h" namespace "hello_world" nogil:
    cdef cppclass _HelloWorld "hello_world::HelloWorld":
        _HelloWorld()
        void register_variants(_Library)

ctypedef _Library *LibraryPtr

cdef class HelloWorld:
    cdef readonly:
        LocalTaskID TASK_ID

    def __cinit__(self) -> None:
        self.TASK_ID = _TASK_ID

    cpdef void register_variants(self, object lib):
        # Cython doesn't know that raw_handle is a _Library*, because it cannot
        # see the Cython declarations, so we have to force it
        cdef LibraryPtr lib_ptr = <LibraryPtr>(<long long?>(lib.raw_handle))

        _HelloWorld().register_variants(lib_ptr[0])
