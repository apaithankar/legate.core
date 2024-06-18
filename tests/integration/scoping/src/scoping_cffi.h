/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#ifndef __REGISTER_C__
#define __REGISTER_C__

#ifdef __cplusplus
extern "C" {
#endif

enum RegistryOpCode {
  MULTI_VARIANT    = 0,
  CPU_VARIANT_ONLY = 1,
  MAP_CHECK        = 2,
};

void perform_registration(void);

#ifdef __cplusplus
}
#endif

#endif  // __REGISTER_C__
