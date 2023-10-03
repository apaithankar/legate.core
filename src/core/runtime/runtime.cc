/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "core/runtime/runtime.h"

#include "core/operation/detail/copy.h"
#include "core/operation/detail/operation.h"
#include "core/operation/detail/task.h"
#include "core/partitioning/detail/constraint.h"
#include "core/runtime/detail/runtime.h"

namespace legate {

extern Logger log_legate;

Library Runtime::find_library(const std::string& library_name) const
{
  return Library(impl_->find_library(library_name, false));
}

std::optional<Library> Runtime::maybe_find_library(const std::string& library_name) const
{
  auto result = impl_->find_library(library_name, true);
  return result != nullptr ? std::optional<Library>(Library(result)) : std::nullopt;
}

Library Runtime::create_library(const std::string& library_name,
                                const ResourceConfig& config,
                                std::unique_ptr<mapping::Mapper> mapper)
{
  return Library(
    impl_->create_library(library_name, config, std::move(mapper), false /*in_callback*/));
}

Library Runtime::find_or_create_library(const std::string& library_name,
                                        const ResourceConfig& config,
                                        std::unique_ptr<mapping::Mapper> mapper,
                                        bool* created)
{
  return Library(impl_->find_or_create_library(
    library_name, config, std::move(mapper), created, false /*in_callback*/));
}

AutoTask Runtime::create_task(Library library, int64_t task_id)
{
  return AutoTask(impl_->create_task(library.impl(), task_id));
}

ManualTask Runtime::create_task(Library library, int64_t task_id, const Shape& launch_shape)
{
  return ManualTask(impl_->create_task(library.impl(), task_id, launch_shape));
}

void Runtime::issue_copy(LogicalStore target,
                         LogicalStore source,
                         std::optional<ReductionOpKind> redop)
{
  auto op = redop ? std::make_optional(static_cast<int32_t>(redop.value())) : std::nullopt;
  impl_->issue_copy(target.impl(), source.impl(), op);
}

void Runtime::issue_copy(LogicalStore target, LogicalStore source, std::optional<int32_t> redop)
{
  impl_->issue_copy(target.impl(), source.impl(), redop);
}

void Runtime::issue_gather(LogicalStore target,
                           LogicalStore source,
                           LogicalStore source_indirect,
                           std::optional<ReductionOpKind> redop)
{
  auto op = redop ? std::make_optional(static_cast<int32_t>(redop.value())) : std::nullopt;
  impl_->issue_gather(target.impl(), source.impl(), source_indirect.impl(), op);
}

void Runtime::issue_gather(LogicalStore target,
                           LogicalStore source,
                           LogicalStore source_indirect,
                           std::optional<int32_t> redop)
{
  impl_->issue_gather(target.impl(), source.impl(), source_indirect.impl(), redop);
}

void Runtime::issue_scatter(LogicalStore target,
                            LogicalStore target_indirect,
                            LogicalStore source,
                            std::optional<ReductionOpKind> redop)
{
  auto op = redop ? std::make_optional(static_cast<int32_t>(redop.value())) : std::nullopt;
  impl_->issue_scatter(target.impl(), target_indirect.impl(), source.impl(), op);
}

void Runtime::issue_scatter(LogicalStore target,
                            LogicalStore target_indirect,
                            LogicalStore source,
                            std::optional<int32_t> redop)
{
  impl_->issue_scatter(target.impl(), target_indirect.impl(), source.impl(), redop);
}

void Runtime::issue_scatter_gather(LogicalStore target,
                                   LogicalStore target_indirect,
                                   LogicalStore source,
                                   LogicalStore source_indirect,
                                   std::optional<ReductionOpKind> redop)
{
  auto op = redop ? std::make_optional(static_cast<int32_t>(redop.value())) : std::nullopt;
  impl_->issue_scatter_gather(
    target.impl(), target_indirect.impl(), source.impl(), source_indirect.impl(), op);
}

void Runtime::issue_scatter_gather(LogicalStore target,
                                   LogicalStore target_indirect,
                                   LogicalStore source,
                                   LogicalStore source_indirect,
                                   std::optional<int32_t> redop)
{
  impl_->issue_scatter_gather(
    target.impl(), target_indirect.impl(), source.impl(), source_indirect.impl(), redop);
}

void Runtime::issue_fill(LogicalStore lhs, LogicalStore value)
{
  impl_->issue_fill(lhs.impl(), value.impl());
}

void Runtime::issue_fill(LogicalStore lhs, const Scalar& value)
{
  issue_fill(std::move(lhs), create_store(value));
}

LogicalStore Runtime::tree_reduce(Library library,
                                  int64_t task_id,
                                  LogicalStore store,
                                  int64_t radix)
{
  auto out_store = create_store(store.type(), 1);
  impl_->tree_reduce(library.impl(), task_id, store.impl(), out_store.impl(), radix);
  return out_store;
}

void Runtime::submit(AutoTask&& task) { impl_->submit(std::move(task.impl_)); }

void Runtime::submit(ManualTask&& task) { impl_->submit(std::move(task.impl_)); }

LogicalArray Runtime::create_array(const Type& type, uint32_t dim, bool nullable)
{
  return LogicalArray(impl_->create_array(type.impl(), dim, nullable));
}

LogicalArray Runtime::create_array(const Shape& extents,
                                   const Type& type,
                                   bool nullable,
                                   bool optimize_scalar)
{
  return LogicalArray(impl_->create_array(extents, type.impl(), nullable, optimize_scalar));
}

LogicalArray Runtime::create_array_like(const LogicalArray& to_mirror, std::optional<Type> type)
{
  auto ty = type ? type.value().impl() : to_mirror.type().impl();
  return LogicalArray(impl_->create_array_like(to_mirror.impl(), std::move(ty)));
}

LogicalStore Runtime::create_store(const Type& type, uint32_t dim)
{
  return LogicalStore(impl_->create_store(type.impl(), dim));
}

LogicalStore Runtime::create_store(const Shape& extents,
                                   const Type& type,
                                   bool optimize_scalar /*=false*/)
{
  return LogicalStore(impl_->create_store(extents, type.impl(), optimize_scalar));
}

LogicalStore Runtime::create_store(const Scalar& scalar)
{
  return LogicalStore(impl_->create_store(*scalar.impl_));
}

uint32_t Runtime::max_pending_exceptions() const { return impl_->max_pending_exceptions(); }

void Runtime::set_max_pending_exceptions(uint32_t max_pending_exceptions)
{
  impl_->set_max_pending_exceptions(max_pending_exceptions);
}

void Runtime::raise_pending_task_exception() { impl_->raise_pending_task_exception(); }

std::optional<TaskException> Runtime::check_pending_task_exception()
{
  return impl_->check_pending_task_exception();
}

void Runtime::issue_execution_fence(bool block /*=false*/) { impl_->issue_execution_fence(block); }

mapping::Machine Runtime::get_machine() const { return mapping::Machine(impl_->get_machine()); }

/*static*/ Runtime* Runtime::get_runtime()
{
  static Runtime* the_runtime{nullptr};
  if (nullptr == the_runtime) {
    auto* impl = detail::Runtime::get_runtime();
    if (!impl->initialized())
      throw std::runtime_error(
        "Legate runtime has not been initialized. Please invoke legate::start to use the runtime");

    the_runtime = new Runtime(impl);
  }
  return the_runtime;
}

Runtime::~Runtime() {}

Runtime::Runtime(detail::Runtime* impl) : impl_(impl) {}

int32_t start(int32_t argc, char** argv) { return detail::Runtime::start(argc, argv); }

int32_t finish() { return detail::Runtime::get_runtime()->finish(); }

mapping::Machine get_machine() { return Runtime::get_runtime()->get_machine(); }

}  // namespace legate

extern "C" {

void legate_core_perform_registration()
{
  // Tell the runtime about our registration callback so we can register ourselves
  // Make sure it is global so this shared object always gets loaded on all nodes
  Legion::Runtime::perform_registration_callback(legate::detail::initialize_core_library_callback,
                                                 true /*global*/);
  legate::detail::Runtime::get_runtime()->initialize(Legion::Runtime::get_context());
}
}
