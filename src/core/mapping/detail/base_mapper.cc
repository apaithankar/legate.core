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

#include "core/mapping/detail/base_mapper.h"

#include "core/mapping/detail/instance_manager.h"
#include "core/mapping/detail/mapping.h"
#include "core/mapping/detail/operation.h"
#include "core/mapping/detail/store.h"
#include "core/mapping/operation.h"
#include "core/runtime/detail/projection.h"
#include "core/runtime/detail/shard.h"
#include "core/utilities/detail/core_ids.h"
#include "core/utilities/detail/enumerate.h"
#include "core/utilities/detail/env.h"
#include "core/utilities/detail/formatters.h"
#include "core/utilities/detail/type_traits.h"
#include "core/utilities/detail/zip.h"
#include "core/utilities/linearize.h"

#include "mappers/mapping_utilities.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace legate::mapping::detail {

namespace {

const std::vector<StoreTarget>& default_store_targets(Processor::Kind kind)
{
  switch (kind) {
    case Processor::Kind::TOC_PROC: {
      static const std::vector<StoreTarget> targets = {StoreTarget::FBMEM, StoreTarget::ZCMEM};
      return targets;
    }
    case Processor::Kind::OMP_PROC: {
      static const std::vector<StoreTarget> targets = {StoreTarget::SOCKETMEM, StoreTarget::SYSMEM};
      return targets;
    }
    case Processor::Kind::LOC_PROC: {
      static const std::vector<StoreTarget> targets = {StoreTarget::SYSMEM};
      return targets;
    }
    default: break;
  }
  LEGATE_ABORT("Could not find ProcessorKind " << static_cast<int>(kind)
                                               << " in default store targets");
}

std::string log_mappable(const Legion::Mappable& mappable, bool prefix_only = false)
{
  static const std::unordered_map<Legion::MappableType, std::string> prefixes = {
    {LEGION_TASK_MAPPABLE, "Task "},
    {LEGION_COPY_MAPPABLE, "Copy "},
    {LEGION_INLINE_MAPPABLE, "Inline mapping "},
    {LEGION_PARTITION_MAPPABLE, "Partition "},
  };
  auto finder = prefixes.find(mappable.get_mappable_type());
  LEGATE_ASSERT(finder != prefixes.end());
  if (prefix_only) {
    return finder->second;
  }

  return fmt::format("{}{}", finder->second, mappable.get_unique_id());
}

}  // namespace

BaseMapper::BaseMapper(mapping::Mapper* legate_mapper,
                       Legion::Mapping::MapperRuntime* _mapper_runtime,
                       const legate::detail::Library* _library)
  : Mapper{_mapper_runtime},
    legate_mapper_{legate_mapper},
    mapper_runtime{_mapper_runtime},
    legion_machine{Legion::Machine::get_machine()},
    library{_library},
    logger{create_logger_name_()},
    local_instances_{InstanceManager::get_instance_manager()},
    reduction_instances_{ReductionInstanceManager::get_instance_manager()},
    mapper_name_{fmt::format("{} on Node {}", library->get_library_name(), local_machine_.node_id)}
{
  legate_mapper_->set_machine(this);
}

BaseMapper::~BaseMapper()
{
  // Compute the size of all our remaining instances in each memory
  if (legate::detail::LEGATE_SHOW_USAGE.get().value_or(false)) {
    auto mem_sizes             = local_instances_->aggregate_instance_sizes();
    const char* memory_kinds[] = {
#define MEM_NAMES(name, desc) desc,
      REALM_MEMORY_KINDS(MEM_NAMES)
#undef MEM_NAMES
    };
    for (auto&& pair : mem_sizes) {
      const auto& mem            = pair.first;
      const std::size_t capacity = mem.capacity();
      logger.print(
        "%s used %ld bytes of %s memory %llx with "
        "%ld total bytes (%.2g%%)",
        library->get_library_name().data(),
        pair.second,
        memory_kinds[mem.kind()],
        mem.id,
        capacity,
        100.0 * static_cast<double>(pair.second) / static_cast<double>(capacity));
    }
  }
}

std::string BaseMapper::create_logger_name_() const
{
  return fmt::format("{}.mapper", library->get_library_name());
}

void BaseMapper::select_task_options(Legion::Mapping::MapperContext ctx,
                                     const Legion::Task& task,
                                     TaskOptions& output)
{
  Task legate_task(&task, library, runtime, ctx);
  auto hi = task.index_domain.hi();
  auto lo = task.index_domain.lo();
  for (auto&& array : legate_task.inputs()) {
    auto stores = array->stores();
    for (auto&& store : stores) {
      if (store->is_future()) {
        continue;
      }
      auto idx   = store->requirement_index();
      auto&& req = task.regions[idx];
      for (auto&& d : store->find_imaginary_dims()) {
        if ((hi[d] - lo[d]) >= 1) {
          output.check_collective_regions.insert(idx);
          break;
        }
      }
      if (task.index_domain.get_volume() > 1 &&
          req.partition == Legion::LogicalPartition::NO_PART) {
        output.check_collective_regions.insert(idx);
      }
    }
  }
  for (auto&& array : legate_task.reductions()) {
    auto stores = array->stores();
    for (auto&& store : stores) {
      if (store->is_future()) {
        continue;
      }
      auto idx   = store->requirement_index();
      auto&& req = task.regions[idx];
      if (req.privilege & LEGION_WRITE_PRIV) {
        continue;
      }
      if (req.handle_type == LEGION_SINGULAR_PROJECTION || req.projection != 0) {
        output.check_collective_regions.insert(idx);
      }
    }
  }

  auto& machine_desc = legate_task.machine();
  auto&& all_targets = machine_desc.valid_targets();

  std::vector<TaskTarget> options;
  options.reserve(all_targets.size());
  for (auto&& target : all_targets) {
    if (has_variant_(ctx, task, target)) {
      options.push_back(target);
    }
  }
  if (options.empty()) {
    LEGATE_ABORT("Task " << task.get_task_name() << "[" << task.get_provenance_string()
                         << "] does not have a valid variant "
                         << "for this resource configuration: " << machine_desc);
  }

  auto target = legate_mapper_->task_target(mapping::Task(&legate_task), options);
  // The initial processor just needs to have the same kind as the eventual target of this task
  output.initial_proc = local_machine_.procs(target).front();

  // We never want valid instances
  output.valid_instances = false;
}

void BaseMapper::premap_task(Legion::Mapping::MapperContext /*ctx*/,
                             const Legion::Task& /*task*/,
                             const PremapTaskInput& /*input*/,
                             PremapTaskOutput& /*output*/)
{
  // NO-op since we know that all our futures should be mapped in the system memory
}

void BaseMapper::slice_task(Legion::Mapping::MapperContext ctx,
                            const Legion::Task& task,
                            const SliceTaskInput& input,
                            SliceTaskOutput& output)
{
  const Task legate_task{&task, library, runtime, ctx};

  auto& machine_desc = legate_task.machine();
  auto local_range   = local_machine_.slice(legate_task.target(), machine_desc);

  Legion::ProjectionID projection = 0;
  for (auto&& req : task.regions) {
    if (req.tag == traits::detail::to_underlying(legate::detail::CoreMappingTag::KEY_STORE)) {
      projection = req.projection;
      break;
    }
  }
  auto key_functor = legate::detail::find_projection_function(projection);

  // For multi-node cases we should already have been sharded so we
  // should just have one or a few points here on this node, so iterate
  // them and round-robin them across the local processors here
  output.slices.reserve(input.domain.get_volume());

  // Get the domain for the sharding space also
  Domain sharding_domain = task.index_domain;
  if (task.sharding_space.exists()) {
    sharding_domain = runtime->get_index_space_domain(ctx, task.sharding_space);
  }

  auto lo                = key_functor->project_point(sharding_domain.lo());
  auto hi                = key_functor->project_point(sharding_domain.hi());
  auto start_proc_id     = machine_desc.processor_range().low;
  auto total_tasks_count = linearize(lo, hi, hi) + 1;

  for (Domain::DomainPointIterator itr{input.domain}; itr; itr++) {
    auto p = key_functor->project_point(itr.p);
    auto idx =
      linearize(lo, hi, p) * local_range.total_proc_count() / total_tasks_count + start_proc_id;
    output.slices.emplace_back(Domain{itr.p, itr.p},
                               local_range[static_cast<std::uint32_t>(idx)],
                               false /*recurse*/,
                               false /*stealable*/);
  }
}

bool BaseMapper::has_variant_(Legion::Mapping::MapperContext ctx,
                              const Legion::Task& task,
                              TaskTarget target)
{
  return find_variant_(ctx, task, to_kind(target)).has_value();
}

std::optional<Legion::VariantID> BaseMapper::find_variant_(Legion::Mapping::MapperContext ctx,
                                                           const Legion::Task& task,
                                                           Processor::Kind kind)
{
  const VariantCacheKey key{task.task_id, kind};

  auto finder = variants_.find(key);
  if (finder != variants_.end()) {
    return finder->second;
  }

  // Haven't seen it before so let's look it up to make sure it exists
  std::vector<Legion::VariantID> avail_variants;
  runtime->find_valid_variants(ctx, key.first, avail_variants, key.second);
  std::optional<Legion::VariantID> result;
  for (auto vid : avail_variants) {
    LEGATE_ASSERT(vid > 0);
    switch (VariantCode{vid}) {
      case VariantCode::CPU: [[fallthrough]];
      case VariantCode::OMP: [[fallthrough]];
      case VariantCode::GPU: {
        result = vid;
        break;
      }
      default: LEGATE_ABORT("Unhandled variant kind " << vid);  // unhandled variant kind
    }
  }
  variants_[key] = result;
  return result;
}

void BaseMapper::map_task(Legion::Mapping::MapperContext ctx,
                          const Legion::Task& task,
                          const MapTaskInput& /*input*/,
                          MapTaskOutput& output)
{
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    logger.debug() << "Entering map_task for "
                   << Legion::Mapping::Utilities::to_string(runtime, ctx, task);
  }

  // Should never be mapping the top-level task here
  LEGATE_CHECK(task.get_depth() > 0);

  // Let's populate easy outputs first
  auto variant = find_variant_(ctx, task, task.target_proc.kind());
  LEGATE_CHECK(variant.has_value());
  output.chosen_variant = *variant;

  Task legate_task(&task, library, runtime, ctx);

  output.task_priority      = legate_task.priority();
  output.copy_fill_priority = legate_task.priority();

  if (task.is_index_space) {
    // If this is an index task, point tasks already have the right targets, so we just need to
    // copy them to the mapper output
    output.target_procs.push_back(task.target_proc);
  } else {
    // If this is a single task, here is the right place to compute the final target processor
    auto local_range =
      local_machine_.slice(legate_task.target(), legate_task.machine(), task.local_function);
    LEGATE_ASSERT(!local_range.empty());
    output.target_procs.push_back(local_range.first());
  }

  const auto& options = default_store_targets(task.target_proc.kind());

  auto client_mappings = legate_mapper_->store_mappings(mapping::Task(&legate_task), options);

  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    const auto validate_colocation = [&](const auto* mapping) {
      auto* first_store = mapping->stores.front();
      for (auto it = mapping->stores.begin() + 1; it != mapping->stores.end(); ++it) {
        if (!(*it)->can_colocate_with(*first_store)) {
          LEGATE_ABORT("Mapper " << get_mapper_name()
                                 << " tried to colocate stores that cannot colocate");
        }
      }
      LEGATE_CHECK(!(mapping->for_future() || mapping->for_unbound_store()) ||
                   mapping->stores.size() == 1);
    };

    for (auto&& client_mapping : client_mappings) {
      validate_colocation(client_mapping.impl());
    }
  }

  std::vector<std::unique_ptr<StoreMapping>> for_futures;
  std::vector<std::unique_ptr<StoreMapping>> for_unbound_stores;
  std::vector<std::unique_ptr<StoreMapping>> for_stores;
  std::map<uint32_t, const StoreMapping*> mapped_futures;
  std::set<RegionField::Id> mapped_regions;

  for (auto&& client_mapping : client_mappings) {
    auto* mapping = client_mapping.impl();
    if (mapping->for_future()) {
      auto fut_idx = mapping->store()->future_index();
      // Only need to map Future-backed Stores corresponding to inputs (i.e. one of task.futures)
      if (fut_idx >= task.futures.size()) {
        continue;
      }
      auto finder = mapped_futures.find(fut_idx);
      if (finder != mapped_futures.end() && finder->second->policy != mapping->policy) {
        LEGATE_ABORT("Mapper " << get_mapper_name() << " returned duplicate store mappings");
      } else {
        mapped_futures.insert({fut_idx, mapping});
        for_futures.emplace_back(client_mapping.release_());
      }
    } else if (mapping->for_unbound_store()) {
      mapped_regions.insert(mapping->store()->unique_region_field_id());
      for_unbound_stores.emplace_back(client_mapping.release_());
    } else {
      for (const auto* store : mapping->stores) {
        mapped_regions.insert(store->unique_region_field_id());
      }
      for_stores.emplace_back(client_mapping.release_());
    }
  }
  client_mappings.clear();

  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    std::map<RegionField::Id, InstanceMappingPolicy> policies;

    for (const auto& mapping : for_stores) {
      for (const auto& store : mapping->stores) {
        auto key          = store->unique_region_field_id();
        const auto finder = policies.find(key);

        if (policies.end() == finder) {
          policies[key] = mapping->policy;
        } else if (mapping->policy != finder->second) {
          LEGATE_ABORT("Mapper " << get_mapper_name() << " returned inconsistent store mappings");
        }
      }
    }
  }

  // Generate default mappings for stores that are not yet mapped by the client mapper
  auto default_option            = options.front();
  auto generate_default_mappings = [&](auto& arrays) {
    for (auto&& array : arrays) {
      auto stores = array->stores();
      for (auto&& store : stores) {
        auto mapping = StoreMapping::default_mapping(store.get(), default_option);
        if (store->is_future()) {
          auto fut_idx = store->future_index();
          // Only need to map Future-backed Stores corresponding to inputs (i.e. one of
          // task.futures)
          if (fut_idx >= task.futures.size()) {
            continue;
          }
          if (mapped_futures.find(fut_idx) != mapped_futures.end()) {
            continue;
          }
          mapped_futures.insert({fut_idx, mapping.get()});
          for_futures.push_back(std::move(mapping));
        } else {
          auto key = store->unique_region_field_id();
          if (mapped_regions.find(key) != mapped_regions.end()) {
            continue;
          }
          mapped_regions.insert(key);
          if (store->unbound()) {
            for_unbound_stores.push_back(std::move(mapping));
          } else {
            for_stores.push_back(std::move(mapping));
          }
        }
      }
    }
  };
  generate_default_mappings(legate_task.inputs());
  generate_default_mappings(legate_task.outputs());
  generate_default_mappings(legate_task.reductions());
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    LEGATE_CHECK(mapped_futures.size() <= task.futures.size());
    // The launching code should be packing all Store-backing Futures first.
    if (!mapped_futures.empty()) {
      const auto max_mapped_fut = mapped_futures.rbegin()->first;

      LEGATE_CHECK(mapped_futures.size() == max_mapped_fut + 1);
    }
  }

  // Map future-backed stores
  output.future_locations.resize(mapped_futures.size());
  for (auto&& mapping : for_futures) {
    auto fut_idx       = mapping->store()->future_index();
    StoreTarget target = mapping->policy.target;
    if (LEGATE_DEFINED(LEGATE_NO_FUTURES_ON_FB)) {
      if (target == StoreTarget::FBMEM) {
        target = StoreTarget::ZCMEM;
      }
    }
    output.future_locations[fut_idx] = local_machine_.get_memory(task.target_proc, target);
  }

  // Map unbound stores
  auto map_unbound_stores = [&](auto& mappings) {
    for (auto&& mapping : mappings) {
      auto req_idx = mapping->requirement_index();
      output.output_targets[req_idx] =
        local_machine_.get_memory(task.target_proc, mapping->policy.target);
      auto ndim = mapping->store()->dim();
      // FIXME: Unbound stores can have more than one dimension later
      std::vector<Legion::DimensionKind> dimension_ordering;

      dimension_ordering.reserve(static_cast<std::size_t>(ndim) + 1);
      for (std::int32_t dim = ndim - 1; dim >= 0; --dim) {
        dimension_ordering.push_back(static_cast<Legion::DimensionKind>(
          static_cast<std::int32_t>(Legion::DimensionKind::LEGION_DIM_X) + dim));
      }
      dimension_ordering.push_back(Legion::DimensionKind::LEGION_DIM_F);
      output.output_constraints[req_idx].ordering_constraint =
        Legion::OrderingConstraint(dimension_ordering, false);
    }
  };
  map_unbound_stores(for_unbound_stores);

  output.chosen_instances.resize(task.regions.size());
  OutputMap output_map;
  for (std::uint32_t idx = 0; idx < task.regions.size(); ++idx) {
    output_map[&task.regions[idx]] = &output.chosen_instances[idx];
  }

  map_legate_stores_(ctx,
                     task,
                     for_stores,
                     task.target_proc,
                     output_map,
                     legate_task.machine().count() < task.index_domain.get_volume());
}

void BaseMapper::replicate_task(Legion::Mapping::MapperContext /*ctx*/,
                                const Legion::Task& /*task*/,
                                const ReplicateTaskInput& /*input*/,
                                ReplicateTaskOutput& /*output*/)

{
  LEGATE_ABORT("Should not be called");
}

void BaseMapper::map_legate_stores_(Legion::Mapping::MapperContext ctx,
                                    const Legion::Mappable& mappable,
                                    std::vector<std::unique_ptr<StoreMapping>>& mappings,
                                    Processor target_proc,
                                    OutputMap& output_map,
                                    bool overdecomposed)
{
  auto try_mapping = [&](bool can_fail) {
    const Legion::Mapping::PhysicalInstance NO_INST{};
    std::vector<Legion::Mapping::PhysicalInstance> instances;

    instances.reserve(mappings.size());
    for (auto&& mapping : mappings) {
      Legion::Mapping::PhysicalInstance result = NO_INST;
      auto reqs                                = mapping->requirements();
      // Point tasks collectively writing to the same region must be doing so via distinct
      // instances. This contract is somewhat difficult to satisfy while the mapper also tries to
      // reuse the existing instance for the region, because when the tasks are mapped to processors
      // with a shared memory, the mapper should reuse the instance for only one of the tasks and
      // not for the others, a logic that is tedious to write correctly. For that reason, we simply
      // give up on reusing instances for regions used in collective writes whenever more than one
      // task can try to reuse the existing instance for the same region. The obvious case where the
      // mapepr can safely reuse the instances is that the region is mapped to a framebuffer and not
      // over-decomposed (i.e., there's a 1-1 mapping between tasks and GPUs).
      const auto must_alloc_collective_writes =
        mappable.get_mappable_type() == Legion::Mappable::TASK_MAPPABLE &&
        (overdecomposed || mapping->policy.target != StoreTarget::FBMEM);
      while (map_legate_store_(ctx,
                               mappable,
                               *mapping,
                               reqs,
                               target_proc,
                               result,
                               can_fail,
                               must_alloc_collective_writes)) {
        if (NO_INST == result) {
          LEGATE_ASSERT(can_fail);
          for (auto&& instance : instances) {
            runtime->release_instance(ctx, instance);
          }
          return false;
        }
        std::stringstream reqs_ss;
        if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
          for (auto req_idx : mapping->requirement_indices()) {
            reqs_ss << " " << req_idx;
          }
        }
        if (runtime->acquire_instance(ctx, result)) {
          if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
            logger.debug() << log_mappable(mappable) << ": acquired instance " << result
                           << " for reqs:" << reqs_ss.str();
          }
          break;
        }
        if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
          logger.debug() << log_mappable(mappable) << ": failed to acquire instance " << result
                         << " for reqs:" << reqs_ss.str();
        }
        if ((*reqs.begin())->redop != 0) {
          const Legion::Mapping::AutoLock lock(ctx, reduction_instances_->manager_lock());
          reduction_instances_->erase(result);
        } else {
          const Legion::Mapping::AutoLock lock(ctx, local_instances_->manager_lock());
          local_instances_->erase(result);
        }
        result = NO_INST;
      }
      instances.push_back(result);
    }

    // If we're here, all stores are mapped and instances are all acquired
    for (std::uint32_t idx = 0; idx < mappings.size(); ++idx) {
      auto& mapping  = mappings[idx];
      auto& instance = instances[idx];
      for (auto&& req : mapping->requirements()) {
        output_map[req]->push_back(instance);
      }
    }
    return true;
  };

  // We can retry the mapping with tightened policies only if at least one of the policies
  // is lenient
  bool can_fail = false;
  for (auto&& mapping : mappings) {
    can_fail = can_fail || !mapping->policy.exact;
  }

  if (!try_mapping(can_fail)) {
    if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
      logger.debug() << log_mappable(mappable) << " failed to map all stores, retrying with "
                     << "tighter policies";
    }
    // If instance creation failed we try mapping all stores again, but request tight instances for
    // write requirements. The hope is that these write requirements cover the entire region (i.e.
    // they use a complete partition), so the new tight instances will invalidate any pre-existing
    // "bloated" instances for the same region, freeing up enough memory so that mapping can succeed
    tighten_write_policies_(mappable, mappings);
    try_mapping(false);
  }
}

void BaseMapper::tighten_write_policies_(const Legion::Mappable& mappable,
                                         const std::vector<std::unique_ptr<StoreMapping>>& mappings)
{
  for (auto&& mapping : mappings) {
    // If the policy is exact, there's nothing we can tighten
    if (mapping->policy.exact) {
      continue;
    }

    auto priv = traits::detail::to_underlying(LEGION_NO_ACCESS);
    for (const auto* req : mapping->requirements()) {
      priv |= req->privilege;
    }
    // We tighten only write requirements
    if (!(priv & LEGION_WRITE_PRIV)) {
      continue;
    }

    if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
      std::stringstream reqs_ss;
      for (auto req_idx : mapping->requirement_indices()) {
        reqs_ss << " " << req_idx;
      }
      logger.debug() << log_mappable(mappable)
                     << ": tightened mapping policy for reqs:" << std::move(reqs_ss).str();
    }
    mapping->policy.exact = true;
  }
}

bool BaseMapper::map_legate_store_(Legion::Mapping::MapperContext ctx,
                                   const Legion::Mappable& mappable,
                                   const StoreMapping& mapping,
                                   const std::set<const Legion::RegionRequirement*>& reqs,
                                   Processor target_proc,
                                   Legion::Mapping::PhysicalInstance& result,
                                   bool can_fail,
                                   bool must_alloc_collective_writes)
{
  if (reqs.empty()) {
    return false;
  }

  std::vector<Legion::LogicalRegion> regions;
  for (auto* req : reqs) {
    if (LEGION_NO_ACCESS == req->privilege) {
      continue;
    }
    regions.push_back(req->region);
  }
  if (regions.empty()) {
    return false;
  }

  const auto& policy = mapping.policy;
  auto target_memory = local_machine_.get_memory(target_proc, policy.target);

  auto redop = GlobalRedopID{(*reqs.begin())->redop};
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    for (auto* req : reqs) {
      if (redop != GlobalRedopID{req->redop}) {
        LEGATE_ABORT(
          "Colocated stores should be either non-reduction arguments "
          "or reductions with the same reduction operator.");
      }
    }
  }
  // Targets of reduction copies should be mapped to normal instances
  if (mappable.get_mappable_type() == Legion::Mappable::COPY_MAPPABLE) {
    redop = GlobalRedopID{0};
  }

  // Generate layout constraints from the store mapping
  Legion::LayoutConstraintSet layout_constraints;
  mapping.populate_layout_constraints(layout_constraints);
  auto& fields = layout_constraints.field_constraint.field_set;

  // If we're making a reduction instance:
  if (redop != GlobalRedopID{0}) {
    // We need to hold the instance manager lock as we're about to try
    // to find an instance
    const Legion::Mapping::AutoLock reduction_lock{ctx, reduction_instances_->manager_lock()};

    // This whole process has to appear atomic
    runtime->disable_reentrant(ctx);

    // reuse reductions only for GPU tasks:
    if (target_proc.kind() == Processor::TOC_PROC) {
      // See if we already have it in our local instances
      if (fields.size() == 1 && regions.size() == 1) {
        auto ret = reduction_instances_->find_instance(
          redop, regions.front(), fields.front(), target_memory, policy);

        if (ret.has_value()) {
          if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
            logger.debug() << "Operation " << mappable.get_unique_id()
                           << ": reused cached reduction instance " << result << " for "
                           << regions.front();
          }
          result = *std::move(ret);
          runtime->enable_reentrant(ctx);
          // Needs acquire to keep the runtime happy
          return true;
        }
      }
    }

    // if we didn't find it, create one
    layout_constraints.add_constraint(Legion::SpecializedConstraint{
      REDUCTION_FOLD_SPECIALIZE, static_cast<Legion::ReductionOpID>(redop)});
    std::size_t footprint = 0;
    if (runtime->create_physical_instance(ctx,
                                          target_memory,
                                          layout_constraints,
                                          regions,
                                          result,
                                          true /*acquire*/,
                                          LEGION_GC_DEFAULT_PRIORITY,
                                          false /*tight bounds*/,
                                          &footprint)) {
      if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
        Realm::LoggerMessage msg = logger.debug();
        msg << "Operation " << mappable.get_unique_id() << ": created reduction instance " << result
            << " for";
        for (auto&& r : regions) {
          msg << " " << r;
        }
        msg << " (size: " << footprint << " bytes, memory: " << target_memory << ")";
      }
      if (target_proc.kind() == Processor::TOC_PROC) {
        // store reduction instance
        if (fields.size() == 1 && regions.size() == 1) {
          auto fid = fields.front();
          reduction_instances_->record_instance(redop, regions.front(), fid, result, policy);
        }
      }
      runtime->enable_reentrant(ctx);
      // Record the operation that created this instance
      if (const auto provenance = mappable.get_provenance_string(); !provenance.empty()) {
        creating_operation_[result] = std::string{provenance};
      }
      // We already did the acquire
      return false;
    }
    runtime->enable_reentrant(ctx);
    if (!can_fail) {
      report_failed_mapping_(ctx, mappable, mapping, target_memory, redop, footprint);
    }
    return true;
  }

  const Legion::Mapping::AutoLock lock{ctx, local_instances_->manager_lock()};
  runtime->disable_reentrant(ctx);

  auto has_collective_write = [](const auto& to_check) {
    return std::any_of(to_check.begin(), to_check.end(), [](const auto& req) {
      return ((req->privilege & LEGION_WRITE_PRIV) != 0) &&
             ((req->prop & LEGION_COLLECTIVE_MASK) != 0);
    });
  };
  const auto alloc_policy = must_alloc_collective_writes && has_collective_write(reqs)
                              ? AllocPolicy::MUST_ALLOC
                              : policy.allocation;

  // See if we already have it in our local instances
  if (fields.size() == 1 && regions.size() == 1 && alloc_policy != AllocPolicy::MUST_ALLOC) {
    auto ret =
      local_instances_->find_instance(regions.front(), fields.front(), target_memory, policy);

    if (ret.has_value()) {
      if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
        logger.debug() << "Operation " << mappable.get_unique_id() << ": reused cached instance "
                       << result << " for " << regions.front();
      }
      result = *std::move(ret);
      runtime->enable_reentrant(ctx);
      // Needs acquire to keep the runtime happy
      return true;
    }
  }

  InternalSharedPtr<RegionGroup> group{nullptr};

  // Haven't made this instance before, so make it now
  if (fields.size() == 1 && regions.size() == 1) {
    // When the client mapper didn't request colocation and also didn't want the instance
    // to be exact, we can do an interesting optimization here to try to reduce unnecessary
    // inter-memory copies. For logical regions that are overlapping we try
    // to accumulate as many as possible into one physical instance and use
    // that instance for all the tasks for the different regions.
    // First we have to see if there is anything we overlap with
    auto fid            = fields.front();
    auto is             = regions.front().get_index_space();
    const Domain domain = runtime->get_index_space_domain(ctx, is);
    group               = local_instances_->find_region_group(
      regions.front(), domain, fid, target_memory, policy.exact);
    regions = group->get_regions();
  }

  bool created          = true;
  bool success          = false;
  std::size_t footprint = 0;

  switch (alloc_policy) {
    case AllocPolicy::MAY_ALLOC: {
      success = runtime->find_or_create_physical_instance(ctx,
                                                          target_memory,
                                                          layout_constraints,
                                                          regions,
                                                          result,
                                                          created,
                                                          true /*acquire*/,
                                                          LEGION_GC_DEFAULT_PRIORITY,
                                                          policy.exact /*tight bounds*/,
                                                          &footprint);
      break;
    }
    case AllocPolicy::MUST_ALLOC: {
      success = runtime->create_physical_instance(ctx,
                                                  target_memory,
                                                  layout_constraints,
                                                  regions,
                                                  result,
                                                  true /*acquire*/,
                                                  LEGION_GC_DEFAULT_PRIORITY,
                                                  policy.exact /*tight bounds*/,
                                                  &footprint);
      break;
    }
    default: LEGATE_ABORT("Should never get here!");
  }

  if (success) {
    // We succeeded in making the instance where we want it
    LEGATE_CHECK(result.exists());
    if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
      if (created) {
        logger.debug() << "Operation " << mappable.get_unique_id() << ": created instance "
                       << result << " for " << *group << " (size: " << footprint
                       << " bytes, memory: " << target_memory << ")";
      } else {
        logger.debug() << "Operation " << mappable.get_unique_id() << ": found instance " << result
                       << " for " << *group;
      }
    }
    // Only save the result for future use if it is not an external instance
    if (!result.is_external_instance() && group != nullptr) {
      LEGATE_CHECK(fields.size() == 1);
      auto fid = fields.front();
      local_instances_->record_instance(group, fid, result, policy);
    }
    runtime->enable_reentrant(ctx);
    // Record the operation that created this instance
    if (const auto provenance = mappable.get_provenance_string(); !provenance.empty()) {
      creating_operation_[result] = std::string{provenance};
    }
    // We made it so no need for an acquire
    return false;
  }
  // Done with the atomic part
  runtime->enable_reentrant(ctx);

  // If we make it here then we failed entirely
  if (!can_fail) {
    report_failed_mapping_(ctx, mappable, mapping, target_memory, redop, footprint);
  }
  return true;
}

void BaseMapper::report_failed_mapping_(Legion::Mapping::MapperContext ctx,
                                        const Legion::Mappable& mappable,
                                        const StoreMapping& mapping,
                                        Memory target_memory,
                                        GlobalRedopID redop,
                                        std::size_t footprint)
{
  std::string_view opname;
  if (mappable.get_mappable_type() == Legion::Mappable::TASK_MAPPABLE) {
    opname = mappable.as_task()->get_task_name();
  }

  std::string_view provenance = mappable.get_provenance_string();
  if (provenance.empty()) {
    provenance = "unknown provenance";
  }

  std::stringstream req_ss;

  if (redop > GlobalRedopID{0}) {
    req_ss << "reduction (" << traits::detail::to_underlying(redop) << ") requirement(s) ";
  } else {
    req_ss << "region requirement(s) ";
  }
  req_ss << fmt::format("{}", mapping.requirement_indices());

  logger.error() << "Failed to allocate " << footprint << " bytes on memory " << std::hex
                 << target_memory.id << std::dec << " (of kind "
                 << Legion::Mapping::Utilities::to_string(target_memory.kind()) << ") for "
                 << req_ss.str() << " of " << log_mappable(mappable, true /*prefix_only*/) << opname
                 << "[" << provenance << "] (UID " << mappable.get_unique_id() << ")";
  for (const Legion::RegionRequirement* req : mapping.requirements()) {
    for (const Legion::FieldID fid : req->instance_fields) {
      logger.error() << "  corresponding to a LogicalStore allocated at "
                     << retrieve_alloc_info_(ctx, req->region.get_field_space(), fid);
    }
  }

  std::vector<Legion::Mapping::PhysicalInstance> existing;

  runtime->find_physical_instances(ctx,
                                   target_memory,
                                   Legion::LayoutConstraintSet{},
                                   std::vector<Legion::LogicalRegion>{},
                                   existing);

  using StoreKey = std::pair<Legion::FieldSpace, Legion::FieldID>;
  std::unordered_map<StoreKey, std::vector<Legion::Mapping::PhysicalInstance>, hasher<StoreKey>>
    insts_for_store{};
  std::size_t total_size = 0;

  for (const Legion::Mapping::PhysicalInstance& inst : existing) {
    std::set<Legion::FieldID> fields;

    total_size += inst.get_instance_size();
    inst.get_fields(fields);
    for (const Legion::FieldID fid : fields) {
      insts_for_store[{inst.get_field_space(), fid}].push_back(inst);
    }
  }

  // TODO(mpapadakis): Once the one-pool solution is merged, we will no longer need to mention eager
  // allocations here, but will have to properly report the case where a task reserving an eager
  // instance (of known size) has not finished yet, so its reserved eager memory has not yet been
  // returned to the deferred pool.
  logger.error() << "There is not enough space because Legate is reserving " << total_size
                 << " of the available " << target_memory.capacity() << " bytes (minus the eager"
                 << " pool allocation) for the following LogicalStores:";
  for (auto&& [store_key, insts] : insts_for_store) {
    auto&& [fs, fid] = store_key;

    logger.error() << "LogicalStore allocated at " << retrieve_alloc_info_(ctx, fs, fid) << ":";
    for (const Legion::Mapping::PhysicalInstance& inst : insts) {
      std::set<Legion::FieldID> fields;
      inst.get_fields(fields);
      logger.error() << "  Instance " << std::hex << inst.get_instance_id() << std::dec
                     << " of size " << inst.get_instance_size() << " covering elements "
                     << Legion::Mapping::Utilities::to_string(
                          runtime, ctx, inst.get_instance_domain())
                     << (fields.size() > 1 ? " of multiple stores" : " ");
      if (const auto it = creating_operation_.find(inst); it != creating_operation_.end()) {
        logger.error() << "    created for an operation launched at " << it->second;
      }
    }
  }

  LEGATE_ABORT("Out of memory");
}

void BaseMapper::select_task_variant(Legion::Mapping::MapperContext ctx,
                                     const Legion::Task& task,
                                     const SelectVariantInput& input,
                                     SelectVariantOutput& output)
{
  auto variant = find_variant_(ctx, task, input.processor.kind());
  // It is checked (just not on optimized builds)
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  LEGATE_ASSERT(variant.has_value());
  // It is checked (just not on optimized builds)
  // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
  output.chosen_variant = *variant;
}

void BaseMapper::postmap_task(Legion::Mapping::MapperContext /*ctx*/,
                              const Legion::Task& /*task*/,
                              const PostMapInput& /*input*/,
                              PostMapOutput& /*output*/)
{
  // We should currently never get this call in Legate
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::select_task_sources(Legion::Mapping::MapperContext ctx,
                                     const Legion::Task& /*task*/,
                                     const SelectTaskSrcInput& input,
                                     SelectTaskSrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

namespace {

using Bandwidth = std::uint32_t;

// Source instance annotated with the memory bandwidth
class AnnotatedSourceInstance {
 public:
  AnnotatedSourceInstance(Legion::Mapping::PhysicalInstance _instance, Bandwidth _bandwidth)
    : instance{std::move(_instance)}, bandwidth{_bandwidth}
  {
  }

  Legion::Mapping::PhysicalInstance instance{};
  Bandwidth bandwidth{};
};

void find_source_instance_bandwidth(std::vector<AnnotatedSourceInstance>* all_sources,
                                    std::unordered_map<Memory, Bandwidth>* source_memory_bandwidths,
                                    const Legion::Mapping::PhysicalInstance& source_instance,
                                    const Memory& target_memory,
                                    const Legion::Machine& legion_machine,
                                    const LocalMachine& local_machine)
{
  auto source_memory = source_instance.get_location();
  auto finder        = source_memory_bandwidths->find(source_memory);

  std::uint32_t bandwidth{0};
  if (source_memory_bandwidths->end() == finder) {
    std::vector<Legion::MemoryMemoryAffinity> affinities;
    legion_machine.get_mem_mem_affinity(
      affinities, source_memory, target_memory, false /*not just local affinities*/);
    // affinities being empty means that there's no direct channel between the source and target
    // memories, in which case we assign the smallest bandwidth
    // TODO(wonchanl): Not all multi-hop copies are equal
    if (!affinities.empty()) {
      LEGATE_ASSERT(affinities.size() == 1);
      bandwidth = affinities.front().bandwidth;
    } else if (source_memory.kind() == Realm::Memory::GPU_FB_MEM) {
      // Last resort: check if this is a special case of a local-node multi-hop CPU<->GPU copy.
      bandwidth = local_machine.g2c_multi_hop_bandwidth(source_memory, target_memory);
    } else if (target_memory.kind() == Realm::Memory::GPU_FB_MEM) {
      // Symmetric case to the above
      bandwidth = local_machine.g2c_multi_hop_bandwidth(target_memory, source_memory);
    }
    (*source_memory_bandwidths)[source_memory] = bandwidth;
  } else {
    bandwidth = finder->second;
  }

  all_sources->emplace_back(source_instance, bandwidth);
}

}  // namespace

void BaseMapper::legate_select_sources_(
  Legion::Mapping::MapperContext ctx,
  const Legion::Mapping::PhysicalInstance& target,
  const std::vector<Legion::Mapping::PhysicalInstance>& sources,
  const std::vector<Legion::Mapping::CollectiveView>& collective_sources,
  std::deque<Legion::Mapping::PhysicalInstance>& ranking)
{
  std::unordered_map<Memory, Bandwidth> source_memory_bandwidths;
  // For right now we'll rank instances by the bandwidth of the memory
  // they are in to the destination.
  // TODO(wonchanl): consider layouts when ranking source to help out the DMA system
  auto target_memory = target.get_location();
  // fill in a vector of the sources with their bandwidths
  std::vector<AnnotatedSourceInstance> all_sources;

  all_sources.reserve(sources.size() + collective_sources.size());
  for (auto&& source : sources) {
    find_source_instance_bandwidth(&all_sources,
                                   &source_memory_bandwidths,
                                   source,
                                   target_memory,
                                   legion_machine,
                                   local_machine_);
  }

  for (auto&& collective_source : collective_sources) {
    std::vector<Legion::Mapping::PhysicalInstance> source_instances;

    collective_source.find_instances_nearest_memory(target_memory, source_instances);
    // there must exist at least one instance in the collective view
    LEGATE_ASSERT(!source_instances.empty());
    // we need only first instance if there are several
    find_source_instance_bandwidth(&all_sources,
                                   &source_memory_bandwidths,
                                   source_instances.front(),
                                   target_memory,
                                   legion_machine,
                                   local_machine_);
  }
  LEGATE_ASSERT(!all_sources.empty());
  if (LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    logger.debug() << "Selecting sources for "
                   << Legion::Mapping::Utilities::to_string(runtime, ctx, target);
    for (auto&& [i, src] : legate::detail::enumerate(all_sources)) {
      logger.debug() << ((i < static_cast<std::int64_t>(sources.size())) ? "Standalone"
                                                                         : "Collective")
                     << " source "
                     << Legion::Mapping::Utilities::to_string(runtime, ctx, src.instance)
                     << " bandwidth " << src.bandwidth;
    }
  }

  // Sort source instances by their bandwidths
  std::sort(all_sources.begin(), all_sources.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.bandwidth > rhs.bandwidth;
  });
  // Record all instances from the one of the largest bandwidth to that of the smallest
  for (auto&& source : all_sources) {
    ranking.emplace_back(source.instance);
  }
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext,
                                  const Legion::Task&,
                                  const TaskProfilingInfo&)
{
  LEGATE_ABORT("Shouldn't get any profiling feedback currently");
}

Legion::ShardingID BaseMapper::find_mappable_sharding_functor_id_(const Legion::Mappable& mappable)
{
  const Mappable legate_mappable{&mappable};

  return static_cast<Legion::ShardingID>(legate_mappable.sharding_id());
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext,
                                         const Legion::Task& task,
                                         const SelectShardingFunctorInput&,
                                         SelectShardingFunctorOutput& output)
{
  output.chosen_functor = find_mappable_sharding_functor_id_(task);
}

void BaseMapper::map_inline(Legion::Mapping::MapperContext ctx,
                            const Legion::InlineMapping& inline_op,
                            const MapInlineInput& /*input*/,
                            MapInlineOutput& output)
{
  Processor target_proc{Processor::NO_PROC};
  if (local_machine_.has_omps()) {
    target_proc = local_machine_.omps().front();
  } else {
    target_proc = local_machine_.cpus().front();
  }

  auto store_target = default_store_targets(target_proc.kind()).front();

  LEGATE_ASSERT(inline_op.requirement.instance_fields.size() == 1);

  const Store store{mapper_runtime, ctx, &inline_op.requirement};
  std::vector<std::unique_ptr<StoreMapping>> mappings;

  auto&& reqs = mappings.emplace_back(StoreMapping::default_mapping(&store, store_target, false))
                  ->requirements();

  OutputMap output_map;

  output_map.reserve(reqs.size());
  for (auto* req : reqs) {
    output_map[req] = &output.chosen_instances;
  }

  map_legate_stores_(ctx, inline_op, mappings, target_proc, output_map);
}

void BaseMapper::select_inline_sources(Legion::Mapping::MapperContext ctx,
                                       const Legion::InlineMapping& /*inline_op*/,
                                       const SelectInlineSrcInput& input,
                                       SelectInlineSrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::InlineMapping& /*inline_op*/,
                                  const InlineProfilingInfo& /*input*/)
{
  LEGATE_ABORT("No profiling yet for inline mappings");
}

void BaseMapper::map_copy(Legion::Mapping::MapperContext ctx,
                          const Legion::Copy& copy,
                          const MapCopyInput& /*input*/,
                          MapCopyOutput& output)
{
  const Copy legate_copy{&copy, runtime, ctx};
  output.copy_fill_priority = legate_copy.priority();

  auto& machine_desc = legate_copy.machine();
  auto copy_target   = [&]() {
    // If we're mapping an indirect copy and have data resident in GPU memory,
    // map everything to CPU memory, as indirect copies on GPUs are currently
    // extremely slow.
    auto indirect =
      !copy.src_indirect_requirements.empty() || !copy.dst_indirect_requirements.empty();
    auto&& valid_targets = indirect ? machine_desc.valid_targets_except({TaskTarget::GPU})
                                      : machine_desc.valid_targets();
    // However, if the machine in the scope doesn't have any CPU or OMP as a fallback for
    // indirect copies, we have no choice but using GPUs
    if (valid_targets.empty()) {
      LEGATE_ASSERT(indirect);
      return machine_desc.valid_targets().front();
    }
    return valid_targets.front();
  }();

  auto local_range = local_machine_.slice(copy_target, machine_desc, true);
  Processor target_proc;
  if (copy.is_index_space) {
    Domain sharding_domain = copy.index_domain;
    if (copy.sharding_space.exists()) {
      sharding_domain = runtime->get_index_space_domain(ctx, copy.sharding_space);
    }

    // FIXME: We might later have non-identity projections for copy requirements,
    // in which case we should find the key store and use its projection functor
    // for the linearization
    auto* key_functor = legate::detail::find_projection_function(0);
    auto lo           = key_functor->project_point(sharding_domain.lo());
    auto hi           = key_functor->project_point(sharding_domain.hi());
    auto p            = key_functor->project_point(copy.index_point);

    const std::uint32_t start_proc_id     = machine_desc.processor_range().low;
    const std::uint32_t total_tasks_count = linearize(lo, hi, hi) + 1;
    auto idx =
      linearize(lo, hi, p) * local_range.total_proc_count() / total_tasks_count + start_proc_id;
    target_proc = local_range[idx];
  } else {
    target_proc = local_range.first();
  }

  auto store_target = default_store_targets(target_proc.kind()).front();

  OutputMap output_map;
  auto add_to_output_map =
    [&output_map](const std::vector<Legion::RegionRequirement>& reqs,
                  std::vector<std::vector<Legion::Mapping::PhysicalInstance>>& instances) {
      instances.resize(reqs.size());
      for (auto&& [req, inst] : legate::detail::zip_equal(reqs, instances)) {
        output_map[&req] = &inst;
      }
    };
  add_to_output_map(copy.src_requirements, output.src_instances);
  add_to_output_map(copy.dst_requirements, output.dst_instances);

  LEGATE_ASSERT(copy.src_indirect_requirements.size() <= 1);
  LEGATE_ASSERT(copy.dst_indirect_requirements.size() <= 1);
  if (!copy.src_indirect_requirements.empty()) {
    // This is to make the push_back call later add the instance to the right place
    output.src_indirect_instances.clear();
    output_map[&copy.src_indirect_requirements.front()] = &output.src_indirect_instances;
  }
  if (!copy.dst_indirect_requirements.empty()) {
    // This is to make the push_back call later add the instance to the right place
    output.dst_indirect_instances.clear();
    output_map[&copy.dst_indirect_requirements.front()] = &output.dst_indirect_instances;
  }

  auto&& inputs     = legate_copy.inputs();
  auto&& outputs    = legate_copy.outputs();
  auto&& input_ind  = legate_copy.input_indirections();
  auto&& output_ind = legate_copy.output_indirections();

  const auto stores_to_copy = {
    std::ref(inputs), std::ref(outputs), std::ref(input_ind), std::ref(output_ind)};

  std::size_t reserve_size = 0;
  for (auto&& store : stores_to_copy) {
    reserve_size += store.get().size();
  }

  std::vector<std::unique_ptr<StoreMapping>> mappings;

  mappings.reserve(reserve_size);
  for (auto&& store_set : stores_to_copy) {
    for (auto&& store : store_set.get()) {
      mappings.emplace_back(StoreMapping::default_mapping(&store, store_target, false));
    }
  }
  map_legate_stores_(ctx, copy, mappings, target_proc, output_map);
}

void BaseMapper::select_copy_sources(Legion::Mapping::MapperContext ctx,
                                     const Legion::Copy& /*copy*/,
                                     const SelectCopySrcInput& input,
                                     SelectCopySrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::Copy& /*copy*/,
                                  const CopyProfilingInfo& /*input*/)
{
  LEGATE_ABORT("No profiling for copies yet");
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Copy& copy,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& output)
{
  // TODO(wonchanl): Copies can have key stores in the future
  output.chosen_functor = find_mappable_sharding_functor_id_(copy);
}

void BaseMapper::select_close_sources(Legion::Mapping::MapperContext ctx,
                                      const Legion::Close& /*close*/,
                                      const SelectCloseSrcInput& input,
                                      SelectCloseSrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::Close& /*close*/,
                                  const CloseProfilingInfo& /*input*/)
{
  LEGATE_ABORT("No profiling yet for legate");
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Close& /*close*/,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::map_acquire(Legion::Mapping::MapperContext /*ctx*/,
                             const Legion::Acquire& /*acquire*/,
                             const MapAcquireInput& /*input*/,
                             MapAcquireOutput& /*output*/)
{
  // Nothing to do
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::Acquire& /*acquire*/,
                                  const AcquireProfilingInfo& /*input*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Acquire& /*acquire*/,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::map_release(Legion::Mapping::MapperContext /*ctx*/,
                             const Legion::Release& /*release*/,
                             const MapReleaseInput& /*input*/,
                             MapReleaseOutput& /*output*/)
{
  // Nothing to do
}

void BaseMapper::select_release_sources(Legion::Mapping::MapperContext ctx,
                                        const Legion::Release& /*release*/,
                                        const SelectReleaseSrcInput& input,
                                        SelectReleaseSrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::Release& /*release*/,
                                  const ReleaseProfilingInfo& /*input*/)
{
  // No profiling for legate yet
  LEGATE_ABORT("No profiling for legate yet");
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Release& /*release*/,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::select_partition_projection(Legion::Mapping::MapperContext /*ctx*/,
                                             const Legion::Partition& /*partition*/,
                                             const SelectPartitionProjectionInput& input,
                                             SelectPartitionProjectionOutput& output)
{
  // If we have an open complete partition then use it
  if (!input.open_complete_partitions.empty()) {
    output.chosen_partition = input.open_complete_partitions.front();
  } else {
    output.chosen_partition = Legion::LogicalPartition::NO_PART;
  }
}

void BaseMapper::map_partition(Legion::Mapping::MapperContext ctx,
                               const Legion::Partition& partition,
                               const MapPartitionInput&,
                               MapPartitionOutput& output)
{
  auto target_proc = [&] {
    if (local_machine_.has_omps()) {
      return local_machine_.omps().front();
    }
    return local_machine_.cpus().front();
  }();

  auto store_target = default_store_targets(target_proc.kind()).front();

  LEGATE_ASSERT(partition.requirement.instance_fields.size() == 1);

  const Store store{mapper_runtime, ctx, &partition.requirement};
  std::vector<std::unique_ptr<StoreMapping>> mappings;

  auto&& reqs = mappings.emplace_back(StoreMapping::default_mapping(&store, store_target, false))
                  ->requirements();

  OutputMap output_map;

  output_map.reserve(reqs.size());
  for (auto* req : reqs) {
    output_map[req] = &output.chosen_instances;
  }

  map_legate_stores_(ctx, partition, mappings, std::move(target_proc), output_map);
}

void BaseMapper::select_partition_sources(Legion::Mapping::MapperContext ctx,
                                          const Legion::Partition& /*partition*/,
                                          const SelectPartitionSrcInput& input,
                                          SelectPartitionSrcOutput& output)
{
  legate_select_sources_(
    ctx, input.target, input.source_instances, input.collective_views, output.chosen_ranking);
}

void BaseMapper::report_profiling(Legion::Mapping::MapperContext /*ctx*/,
                                  const Legion::Partition& /*partition*/,
                                  const PartitionProfilingInfo& /*input*/)
{
  // No profiling yet
  LEGATE_ABORT("No profiling for partition ops yet");
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Partition& partition,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& output)
{
  output.chosen_functor = find_mappable_sharding_functor_id_(partition);
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::Fill& fill,
                                         const SelectShardingFunctorInput& /*input*/,
                                         SelectShardingFunctorOutput& output)
{
  output.chosen_functor = find_mappable_sharding_functor_id_(fill);
}

void BaseMapper::configure_context(Legion::Mapping::MapperContext /*ctx*/,
                                   const Legion::Task& /*task*/,
                                   ContextConfigOutput& /*output*/)
{
  // Use the defaults currently
}

void BaseMapper::map_future_map_reduction(Legion::Mapping::MapperContext /*ctx*/,
                                          const FutureMapReductionInput& input,
                                          FutureMapReductionOutput& output)
{
  output.serdez_upper_bound = LEGATE_MAX_SIZE_SCALAR_RETURN;
  auto& dest_memories       = output.destination_memories;

  if (local_machine_.has_gpus()) {
    // TODO(wonchanl): It's been reported that blindly mapping target instances of future map
    // reductions to framebuffers hurts performance. Until we find a better mapping policy, we guard
    // the current policy with a macro.
    if (LEGATE_DEFINED(LEGATE_MAP_FUTURE_MAP_REDUCTIONS_TO_GPU)) {
      // If this was joining exceptions, we should put instances on a host-visible memory
      // because they need serdez
      if (input.tag ==
          traits::detail::to_underlying(legate::detail::CoreMappingTag::JOIN_EXCEPTION)) {
        dest_memories.push_back(local_machine_.zerocopy_memory());
      } else {
        auto&& fbufs = local_machine_.frame_buffers();

        dest_memories.reserve(fbufs.size());
        for (auto&& pair : fbufs) {
          dest_memories.push_back(pair.second);
        }
      }
    } else {
      dest_memories.push_back(local_machine_.zerocopy_memory());
    }
  } else if (local_machine_.has_socket_memory()) {
    auto&& smems = local_machine_.socket_memories();

    dest_memories.reserve(smems.size());
    for (auto&& pair : smems) {
      dest_memories.push_back(pair.second);
    }
  }
}

void BaseMapper::select_tunable_value(Legion::Mapping::MapperContext /*ctx*/,
                                      const Legion::Task& /*task*/,
                                      const SelectTunableInput& input,
                                      SelectTunableOutput& output)
{
  auto value = legate_mapper_->tunable_value(input.tunable_id);

  output.size = value.size();
  if (output.size) {
    output.value = std::malloc(output.size);
    std::memcpy(output.value, value.ptr(), output.size);
  } else {
    output.value = nullptr;
  }
}

void BaseMapper::select_sharding_functor(Legion::Mapping::MapperContext /*ctx*/,
                                         const Legion::MustEpoch& /*epoch*/,
                                         const SelectShardingFunctorInput& /*input*/,
                                         MustEpochShardingFunctorOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::memoize_operation(Legion::Mapping::MapperContext /*ctx*/,
                                   const Legion::Mappable& /*mappable*/,
                                   const MemoizeInput& /*input*/,
                                   MemoizeOutput& output)
{
  output.memoize = true;
}

void BaseMapper::map_must_epoch(Legion::Mapping::MapperContext /*ctx*/,
                                const MapMustEpochInput& /*input*/,
                                MapMustEpochOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::map_dataflow_graph(Legion::Mapping::MapperContext /*ctx*/,
                                    const MapDataflowGraphInput& /*input*/,
                                    MapDataflowGraphOutput& /*output*/)
{
  LEGATE_ABORT("Should never get here");
}

void BaseMapper::select_tasks_to_map(Legion::Mapping::MapperContext /*ctx*/,
                                     const SelectMappingInput& input,
                                     SelectMappingOutput& output)
{
  // Just map all the ready tasks
  output.map_tasks.insert(input.ready_tasks.begin(), input.ready_tasks.end());
}

void BaseMapper::select_steal_targets(Legion::Mapping::MapperContext /*ctx*/,
                                      const SelectStealingInput& /*input*/,
                                      SelectStealingOutput& /*output*/)
{
  // Nothing to do, no stealing in the leagte mapper currently
}

void BaseMapper::permit_steal_request(Legion::Mapping::MapperContext /*ctx*/,
                                      const StealRequestInput& /*input*/,
                                      StealRequestOutput& /*output*/)
{
  LEGATE_ABORT("no stealing in the legate mapper currently");
}

void BaseMapper::handle_message(Legion::Mapping::MapperContext /*ctx*/,
                                const MapperMessage& /*message*/)
{
  LEGATE_ABORT("We shouldn't be receiving any messages currently");
}

void BaseMapper::handle_task_result(Legion::Mapping::MapperContext /*ctx*/,
                                    const MapperTaskResult& /*result*/)
{
  LEGATE_ABORT("Nothing to do since we should never get one of these");
}

std::string_view BaseMapper::retrieve_alloc_info_(Legion::Mapping::MapperContext ctx,
                                                  Legion::FieldSpace fs,
                                                  Legion::FieldID fid)
{
  constexpr auto tag =
    static_cast<Legion::SemanticTag>(legate::detail::CoreSemanticTag::ALLOC_INFO);
  const void* orig_info;
  std::size_t size;

  if (runtime->retrieve_semantic_information(ctx,
                                             fs,
                                             fid,
                                             tag,
                                             orig_info,
                                             size,
                                             /*can_fail=*/true)) {
    const char* alloc_info = static_cast<const char*>(orig_info);

    if (size > 0 && alloc_info[0] != '\0') {
      return {alloc_info, size};
    }
  }
  return "(unknown provenance)";
}

}  // namespace legate::mapping::detail
