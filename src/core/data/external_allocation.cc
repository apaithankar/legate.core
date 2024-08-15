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

#include "core/data/external_allocation.h"

#include "legate_defines.h"

#include "core/data/detail/external_allocation.h"
#include "core/runtime/detail/runtime.h"

#include "realm/instance.h"

#if LEGATE_DEFINED(LEGATE_USE_CUDA)
#include "realm/cuda/cuda_access.h"
#endif

#include <fmt/format.h>
#include <memory>
#include <stdexcept>

namespace legate {

bool ExternalAllocation::read_only() const { return impl_->read_only(); }

mapping::StoreTarget ExternalAllocation::target() const { return impl_->target(); }

void* ExternalAllocation::ptr() const { return impl_->ptr(); }

std::size_t ExternalAllocation::size() const { return impl_->size(); }

ExternalAllocation::~ExternalAllocation() noexcept = default;

/*static*/ ExternalAllocation ExternalAllocation::create_sysmem(
  void* ptr,
  std::size_t size,
  bool read_only /*=true*/,
  std::optional<Deleter> deleter /*=std::nullopt*/)  // NOLINT(performance-unnecessary-value-param)
{
  auto realm_resource = std::make_unique<Realm::ExternalMemoryResource>(
    reinterpret_cast<std::uintptr_t>(ptr), size, read_only);
  return ExternalAllocation{
    make_internal_shared<detail::ExternalAllocation>(read_only,
                                                     mapping::StoreTarget::SYSMEM,
                                                     ptr,
                                                     size,
                                                     std::move(realm_resource),
                                                     std::move(deleter))};
}

/*static*/ ExternalAllocation ExternalAllocation::create_zcmem(
  void* ptr,
  std::size_t size,
  bool read_only,
  std::optional<Deleter> deleter /*=std::nullopt*/)  // NOLINT(performance-unnecessary-value-param)
{
#if LEGATE_DEFINED(LEGATE_USE_CUDA)
  auto realm_resource = std::make_unique<Realm::ExternalCudaPinnedHostResource>(
    reinterpret_cast<std::uintptr_t>(ptr), size, read_only);
  return ExternalAllocation{
    make_internal_shared<detail::ExternalAllocation>(read_only,
                                                     mapping::StoreTarget::ZCMEM,
                                                     ptr,
                                                     size,
                                                     std::move(realm_resource),
                                                     std::move(deleter))};
#else
  static_cast<void>(ptr);
  static_cast<void>(size);
  static_cast<void>(read_only);
  static_cast<void>(deleter);
  throw std::runtime_error{"CUDA support is unavailable"};
  return {};
#endif
}

/*static*/ ExternalAllocation ExternalAllocation::create_fbmem(
  std::uint32_t local_device_id,
  void* ptr,
  std::size_t size,
  bool read_only,
  std::optional<Deleter> deleter /*=std::nullopt*/)  // NOLINT(performance-unnecessary-value-param)
{
#if LEGATE_DEFINED(LEGATE_USE_CUDA)
  auto& local_gpus = detail::Runtime::get_runtime()->local_machine().gpus();
  if (local_device_id >= local_gpus.size()) {
    throw std::out_of_range{
      fmt::format("Device ID {} is invalid (the runtime is configured with only {}",
                  local_device_id,
                  local_gpus.size())};
  }

  auto realm_resource = std::make_unique<Realm::ExternalCudaMemoryResource>(
    local_device_id, reinterpret_cast<std::uintptr_t>(ptr), size, read_only);
  return ExternalAllocation{
    make_internal_shared<detail::ExternalAllocation>(read_only,
                                                     mapping::StoreTarget::FBMEM,
                                                     ptr,
                                                     size,
                                                     std::move(realm_resource),
                                                     std::move(deleter))};
#else
  static_cast<void>(local_device_id);
  static_cast<void>(ptr);
  static_cast<void>(size);
  static_cast<void>(read_only);
  static_cast<void>(deleter);
  throw std::runtime_error{"CUDA support is unavailable"};
  return {};
#endif
}

/*static*/ ExternalAllocation ExternalAllocation::create_sysmem(
  const void* ptr, std::size_t size, std::optional<Deleter> deleter /*=std::nullopt*/)
{
  return create_sysmem(const_cast<void*>(ptr), size, true, std::move(deleter));
}

/*static*/ ExternalAllocation ExternalAllocation::create_zcmem(
  const void* ptr, std::size_t size, std::optional<Deleter> deleter /*=std::nullopt*/)
{
  return create_zcmem(const_cast<void*>(ptr), size, true, std::move(deleter));
}

/*static*/ ExternalAllocation ExternalAllocation::create_fbmem(
  std::uint32_t local_device_id,
  const void* ptr,
  std::size_t size,
  std::optional<Deleter> deleter /*=std::nullopt*/)
{
  return create_fbmem(local_device_id, const_cast<void*>(ptr), size, true, std::move(deleter));
}

}  // namespace legate
