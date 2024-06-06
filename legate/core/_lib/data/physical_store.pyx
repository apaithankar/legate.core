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


from libc.stdint cimport int32_t

from ..type.type_info cimport Type
from ..utilities.typedefs cimport Domain
from ..utilities.unconstructable cimport Unconstructable
from .inline_allocation cimport InlineAllocation
from .physical_store cimport _PhysicalStore


cdef class PhysicalStore(Unconstructable):
    @staticmethod
    cdef PhysicalStore from_handle(_PhysicalStore handle):
        cdef PhysicalStore result = PhysicalStore.__new__(PhysicalStore)
        result._handle = handle
        return result

    @property
    def ndim(self) -> int32_t:
        return self._handle.dim()

    @property
    def type(self) -> Type:
        return Type.from_handle(self._handle.type())

    @property
    def domain(self) -> Domain:
        return Domain.from_handle(self._handle.domain())

    @property
    def target(self) -> StoreTarget:
        return self._handle.target()

    cpdef InlineAllocation get_inline_allocation(self):
        return InlineAllocation.create(
            self,
            self._handle.get_inline_allocation()
        )

    @property
    def __array_interface__(self) -> dict[str, Any]:
        return self.get_inline_allocation().__array_interface__

    @property
    def __cuda_array_interface__(self) -> dict[str, Any]:
        return self.get_inline_allocation().__cuda_array_interface__
