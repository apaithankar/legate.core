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

#include "core/task/detail/task_context.h"

#include "core/cuda/cuda.h"
#include "core/data/detail/physical_store.h"
#include "core/runtime/detail/runtime.h"
#include "core/utilities/detail/core_ids.h"
#include "core/utilities/detail/deserializer.h"

#include <algorithm>
#include <iterator>

namespace legate::detail {

TaskContext::TaskContext(const Legion::Task* task,
                         VariantCode variant_kind,
                         const std::vector<Legion::PhysicalRegion>& regions)
  : task_{task}, variant_kind_{variant_kind}, regions_{regions}
{
  {
    mapping::detail::MapperDataDeserializer dez{task};
    machine_ = dez.unpack<mapping::detail::Machine>();
  }

  TaskDeserializer dez{task, regions_};
  inputs_     = dez.unpack_arrays();
  outputs_    = dez.unpack_arrays();
  reductions_ = dez.unpack_arrays();
  scalars_    = dez.unpack_scalars();

  // Make copies of stores that we need to postprocess, as clients might move the stores
  // away. Use a temporary vector here to amortize the push-backs from each populate_stores()
  // call.
  std::vector<InternalSharedPtr<PhysicalStore>> stores_cache;
  const auto get_stores = [&](InternalSharedPtr<PhysicalArray>& phys_array)
    -> std::vector<InternalSharedPtr<PhysicalStore>>& {
    stores_cache.clear();
    phys_array->populate_stores(stores_cache);
    return stores_cache;
  };

  for (auto&& output : outputs_) {
    for (auto&& store : get_stores(output)) {
      if (store->is_unbound_store()) {
        unbound_stores_.push_back(std::move(store));
      } else if (store->is_future()) {
        scalar_stores_.push_back(std::move(store));
      }
    }
  }
  for (auto&& reduction : reductions_) {
    auto&& stores = get_stores(reduction);

    std::copy_if(std::make_move_iterator(stores.begin()),
                 std::make_move_iterator(stores.end()),
                 std::back_inserter(scalar_stores_),
                 [](const InternalSharedPtr<PhysicalStore>& store) { return store->is_future(); });
  }

  can_raise_exception_       = dez.unpack<bool>();
  can_elide_device_ctx_sync_ = dez.unpack<bool>();

  bool insert_barrier = false;
  Legion::PhaseBarrier arrival, wait;
  if (task->is_index_space) {
    insert_barrier = dez.unpack<bool>();
    if (insert_barrier) {
      arrival = dez.unpack<Legion::PhaseBarrier>();
      wait    = dez.unpack<Legion::PhaseBarrier>();
    }
    comms_ = dez.unpack<std::vector<legate::comm::Communicator>>();
  }

  // For reduction tree cases, some input stores may be mapped to NO_REGION
  // when the number of subregions isn't a multiple of the chosen radix.
  // To simplify the programming mode, we filter out those "invalid" stores out.
  if (task_->tag == static_cast<Legion::MappingTagID>(CoreMappingTag::TREE_REDUCE)) {
    std::vector<InternalSharedPtr<PhysicalArray>> inputs;
    for (auto&& input : inputs_) {
      if (input->valid()) {
        inputs.push_back(std::move(input));
      }
    }
    inputs_.swap(inputs);
  }

  // CUDA drivers < 520 have a bug that causes deadlock under certain circumstances
  // if the application has multiple threads that launch blocking kernels, such as
  // NCCL all-reduce kernels. This barrier prevents such deadlock by making sure
  // all CUDA driver calls from Realm are done before any of the GPU tasks starts
  // making progress.
  if (insert_barrier) {
    arrival.arrive();
    wait.wait();
  }

  // If the task is running on a GPU and there is at least one scalar store for reduction,
  // we need to wait for all the host-to-device copies for initialization to finish
  if (LEGATE_DEFINED(LEGATE_USE_CUDA) &&
      Processor::get_executing_processor().kind() == Processor::Kind::TOC_PROC) {
    for (auto&& reduction : reductions_) {
      auto reduction_store = reduction->data();
      if (reduction_store->is_future()) {
        LEGATE_CHECK_CUDA(cudaDeviceSynchronize());
        break;
      }
    }
  }
}

void TaskContext::make_all_unbound_stores_empty()
{
  for (auto&& store : unbound_stores_) {
    store->bind_empty_data();
  }
}

TaskReturn TaskContext::pack_return_values() const
{
  auto return_values = get_return_values_();
  if (can_raise_exception()) {
    const ReturnedCppException exn{};

    return_values.push_back(exn.pack());
  }
  return TaskReturn{std::move(return_values)};
}

TaskReturn TaskContext::pack_return_values_with_exception(const ReturnedException& exn) const
{
  auto return_values = get_return_values_();
  if (can_raise_exception()) {
    return_values.push_back(exn.pack());
  }
  return TaskReturn{std::move(return_values)};
}

std::vector<ReturnValue> TaskContext::get_return_values_() const
{
  std::vector<ReturnValue> return_values;

  return_values.reserve(unbound_stores_.size() + scalar_stores_.size() + can_raise_exception());
  for (auto&& store : unbound_stores_) {
    return_values.push_back(store->pack_weight());
  }
  for (auto&& store : scalar_stores_) {
    return_values.push_back(store->pack());
  }

  // If this is a reduction task, we do sanity checks on the invariants
  // the Python code relies on.
  if (task_->tag == static_cast<Legion::MappingTagID>(CoreMappingTag::TREE_REDUCE)) {
    if (return_values.size() != 1 || unbound_stores_.size() != 1) {
      LEGATE_ABORT("Reduction tasks must have only one unbound output and no others");
    }
  }

  return return_values;
}

CUstream_st* TaskContext::get_task_stream() const
{
  return Runtime::get_runtime()->get_cuda_stream();
}

}  // namespace legate::detail
