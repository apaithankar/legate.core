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

#include "core/runtime/detail/library.h"

#include "core/utilities/detail/strtoll.h"

#include "legate.h"
#include "utilities/utilities.h"

#include <gtest/gtest.h>

namespace test_library {

class LibraryMapper : public legate::mapping::Mapper {
  void set_machine(const legate::mapping::MachineQueryInterface* /*machine*/) override {}

  legate::mapping::TaskTarget task_target(
    const legate::mapping::Task& /*task*/,
    const std::vector<legate::mapping::TaskTarget>& options) override
  {
    return options.front();
  }

  std::vector<legate::mapping::StoreMapping> store_mappings(
    const legate::mapping::Task& /*task*/,
    const std::vector<legate::mapping::StoreTarget>& /*options*/) override
  {
    return {};
  }

  legate::Scalar tunable_value(legate::TunableID /*tunable_id*/) override
  {
    LEGATE_ABORT("This method should never be called");
    return legate::Scalar{};
  }
};

using Library = DefaultFixture;

TEST_F(Library, Create)
{
  constexpr std::string_view LIBNAME = "test_library.libA";

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME);

  ASSERT_EQ(lib, runtime->find_library(LIBNAME));

  const auto found_lib = runtime->maybe_find_library(LIBNAME);

  ASSERT_TRUE(found_lib.has_value());
  // We check the optional above ^^^
  ASSERT_EQ(lib, found_lib.value());  // NOLINT(bugprone-unchecked-optional-access)
  ASSERT_EQ(lib.get_library_name(), LIBNAME);
}

TEST_F(Library, FindOrCreate)
{
  constexpr std::string_view LIBNAME  = "test_library.libB";
  constexpr std::string_view LIBNAME1 = "test_library.libB_1";

  auto* runtime = legate::Runtime::get_runtime();

  legate::ResourceConfig config;
  config.max_tasks = 1;

  bool created = false;
  auto p_lib1  = runtime->find_or_create_library(LIBNAME, config, nullptr, {}, &created);
  ASSERT_TRUE(created);

  config.max_tasks = 2;
  auto p_lib2      = runtime->find_or_create_library(LIBNAME, config, nullptr, {}, &created);
  ASSERT_FALSE(created);
  ASSERT_EQ(p_lib1, p_lib2);
  ASSERT_TRUE(p_lib2.valid_task_id(p_lib2.get_task_id(0)));
  ASSERT_THROW(static_cast<void>(p_lib2.get_task_id(1)), std::out_of_range);

  auto p_lib3 = runtime->find_or_create_library(LIBNAME1, config, nullptr, {}, &created);
  ASSERT_TRUE(created);
  ASSERT_NE(p_lib1, p_lib3);
}

TEST_F(Library, FindNonExistent)
{
  constexpr std::string_view LIBNAME = "test_library.libC";

  auto* runtime = legate::Runtime::get_runtime();

  ASSERT_THROW(static_cast<void>(runtime->find_library(LIBNAME)), std::out_of_range);

  ASSERT_EQ(runtime->maybe_find_library(LIBNAME), std::nullopt);
}

TEST_F(Library, InvalidReductionOPID)
{
  using SumReduction_Int32 = legate::SumReduction<std::int32_t>;

  constexpr std::string_view LIBNAME = "test_library.libD";

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME);
  auto local_id = 0;
  ASSERT_THROW(static_cast<void>(lib.register_reduction_operator<SumReduction_Int32>(local_id)),
               std::out_of_range);
}

TEST_F(Library, RegisterReductionOP)
{
  using SumReduction_Int32 = legate::SumReduction<std::int32_t>;

  constexpr std::string_view LIBNAME = "test_library.libE";
  legate::ResourceConfig config;
  config.max_reduction_ops = 1;

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME, config);
  auto local_id = 0;
  auto id       = lib.register_reduction_operator<SumReduction_Int32>(local_id);

  ASSERT_TRUE(lib.valid_reduction_op_id(static_cast<Legion::ReductionOpID>(id)));
  ASSERT_EQ(lib.get_local_reduction_op_id(static_cast<Legion::ReductionOpID>(id)), local_id);
}

TEST_F(Library, RegisterMapper)
{
  constexpr std::string_view LIBNAME = "test_library.libF";

  auto* runtime      = legate::Runtime::get_runtime();
  auto lib           = runtime->create_library(LIBNAME);
  auto* mapper_old   = lib.impl()->get_legion_mapper();
  auto mapper_id_old = lib.get_mapper_id();
  auto mapper        = std::make_unique<LibraryMapper>();
  lib.register_mapper(std::move(mapper));
  auto* mapper_new   = lib.impl()->get_legion_mapper();
  auto mapper_id_new = lib.get_mapper_id();
  ASSERT_EQ(mapper_id_old, mapper_id_new);
  ASSERT_NE(mapper_old, mapper_new);
}

TEST_F(Library, TaskID)
{
  constexpr std::string_view LIBNAME = "test_library.libG";

  legate::ResourceConfig config;
  config.max_tasks = 1;

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME, config);

  auto local_task_id = 0;
  auto task_id       = lib.get_task_id(local_task_id);
  ASSERT_TRUE(lib.valid_task_id(task_id));
  ASSERT_EQ(lib.get_local_task_id(task_id), local_task_id);

  auto task_id_negative = task_id + 1;
  ASSERT_FALSE(lib.valid_task_id(task_id_negative));
}

TEST_F(Library, ProjectID)
{
  constexpr std::string_view LIBNAME = "test_library.libH";

  legate::ResourceConfig config;
  config.max_projections = 2;

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME, config);

  auto local_proj_id_1 = 0;
  auto proj_id_1       = lib.get_projection_id(local_proj_id_1);
  ASSERT_EQ(proj_id_1, 0);
  ASSERT_FALSE(lib.valid_projection_id(proj_id_1));

  auto local_proj_id_2 = 1;
  auto proj_id_2       = lib.get_projection_id(local_proj_id_2);
  ASSERT_TRUE(lib.valid_projection_id(proj_id_2));
  ASSERT_EQ(lib.get_local_projection_id(proj_id_2), local_proj_id_2);

  auto proj_id_negative = proj_id_1 + 2;
  ASSERT_FALSE(lib.valid_projection_id(proj_id_negative));
}

TEST_F(Library, ShardingID)
{
  constexpr std::string_view LIBNAME = "test_library.libI";

  legate::ResourceConfig config;
  config.max_shardings = 1;

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME, config);

  auto local_sharding_id = 0;
  auto sharding_id       = lib.get_sharding_id(local_sharding_id);
  ASSERT_TRUE(lib.valid_sharding_id(sharding_id));
  ASSERT_EQ(lib.get_local_sharding_id(sharding_id), local_sharding_id);

  auto sharding_id_negative = sharding_id + 1;
  ASSERT_FALSE(lib.valid_sharding_id(sharding_id_negative));
}

TEST_F(Library, ResourceIdScopeNegative)
{
  if (!LEGATE_DEFINED(LEGATE_USE_DEBUG)) {
    GTEST_SKIP() << "Skip the test if no LEGATE_USE_DEBUG is defined";
  }

  constexpr std::string_view LIBNAME = "test_library.libJ";

  // Test exception thrown in ResourceIdScope creation function
  legate::ResourceConfig config;
  config.max_tasks     = 1;
  config.max_dyn_tasks = 2;

  auto* runtime = legate::Runtime::get_runtime();
  ASSERT_THROW(static_cast<void>(runtime->create_library(LIBNAME, config)), std::out_of_range);
}

TEST_F(Library, GenerateIDNegative)
{
  constexpr std::string_view LIBNAME = "test_library.libK";

  // Test exception thrown in generate_id
  legate::ResourceConfig config;
  config.max_tasks     = 1;
  config.max_dyn_tasks = 0;

  auto* runtime = legate::Runtime::get_runtime();
  auto lib      = runtime->create_library(LIBNAME, config);
  ASSERT_THROW(static_cast<void>(lib.impl()->get_new_task_id()), std::overflow_error);
}

TEST_F(Library, VariantOptions)
{
  const auto runtime = legate::Runtime::get_runtime();

  const std::map<legate::LegateVariantCode, legate::VariantOptions> default_options1 = {};
  const auto lib1 = runtime->create_library("test_library.foo", {}, nullptr, default_options1);

  ASSERT_EQ(lib1.get_default_variant_options(), default_options1);
  // Repeated calls should get the same thing
  ASSERT_EQ(lib1.get_default_variant_options(), default_options1);

  const std::map<legate::LegateVariantCode, legate::VariantOptions> default_options2 = {
    {LEGATE_CPU_VARIANT, legate::VariantOptions{}.with_return_size(1234)},
    {LEGATE_GPU_VARIANT,
     legate::VariantOptions{}.with_idempotent(true).with_concurrent(true).with_return_size(
       7355608)}};
  const auto lib2 = runtime->create_library("test_library.bar", {}, nullptr, default_options2);

  // Creation of lib2 should not affect lib1
  ASSERT_EQ(lib1.get_default_variant_options(), default_options1);

  ASSERT_EQ(lib2.get_default_variant_options(), default_options2);
  // Repeated calls should get the same thing
  ASSERT_EQ(lib2.get_default_variant_options(), default_options2);
  // Creation of lib2 should not affect lib1
  ASSERT_EQ(lib1.get_default_variant_options(), default_options1);
}

}  // namespace test_library
