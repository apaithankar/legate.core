#=============================================================================
# SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#=============================================================================

list(APPEND CMAKE_MESSAGE_CONTEXT "cpp")

##############################################################################
# - User Options  ------------------------------------------------------------

include(cmake/Modules/legate_core_options.cmake)

##############################################################################
# - Project definition -------------------------------------------------------

# Write the version header
rapids_cmake_write_version_file(include/legate/version_config.hpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/compile_commands.cmake)

legate_core_export_compile_commands()

##############################################################################
# - Build Type ---------------------------------------------------------------

# Set a default build type if none was specified
rapids_cmake_build_type(Release)

##############################################################################
# - conda environment --------------------------------------------------------

rapids_cmake_support_conda_env(conda_env MODIFY_PREFIX_PATH)

# We're building python extension libraries, which must always be installed
# under lib/, even if the system normally uses lib64/. Rapids-cmake currently
# doesn't realize this when we're going through scikit-build, see
# https://github.com/rapidsai/rapids-cmake/issues/426
# Do this before we include Legion, so its build also inherits this setting.
if(TARGET conda_env)
  set(CMAKE_INSTALL_LIBDIR "lib")
endif()

##############################################################################
# - Dependencies -------------------------------------------------------------

# add third party dependencies using CPM
rapids_cpm_init(OVERRIDE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/versions/versions.json)

include(${LEGATE_CORE_DIR}/cmake/Modules/find_or_configure.cmake)

##############################################################################
# - CCCL ---------------------------------------------------------------------

# Pull this in before Legion, so that Legion will use the same libcu++ as
# Legate (the one pull from CCCL)

legate_core_find_or_configure(PACKAGE cccl)

##############################################################################
# - Python -------------------------------------------------------------------

macro(_find_package_Python3)
  rapids_find_package(Python3
    BUILD_EXPORT_SET   legate-core-exports
    INSTALL_EXPORT_SET legate-core-exports
    COMPONENTS         Interpreter Development
    FIND_ARGS
      REQUIRED
  )
  message(VERBOSE "legate.core: Has Python3: ${Python3_FOUND}")
  message(VERBOSE "legate.core: Has Python 3 interpreter: ${Python3_Interpreter_FOUND}")
  message(VERBOSE "legate.core: Python 3 include directories: ${Python3_INCLUDE_DIRS}")
  message(VERBOSE "legate.core: Python 3 libraries: ${Python3_LIBRARIES}")
  message(VERBOSE "legate.core: Python 3 library directories: ${Python3_LIBRARY_DIRS}")
  message(VERBOSE "legate.core: Python 3 version: ${Python3_VERSION}")
endmacro()

##############################################################################
# - Legion -------------------------------------------------------------------

if(Legion_USE_Python)
  _find_package_Python3()
  if(Python3_FOUND AND Python3_VERSION)
    set(Legion_Python_Version ${Python3_VERSION})
  endif()
endif()

include(${LEGATE_CORE_DIR}/cmake/Modules/cuda_arch_helpers.cmake)

if(Legion_USE_CUDA)
  # Needs to run before find_package(Legion)
  set_cuda_arch_from_names()
endif()

###
# If we find Legion already configured on the system, it will report whether it
# was compiled with Python (Legion_USE_PYTHON), CUDA (Legion_USE_CUDA), OpenMP
# (Legion_USE_OpenMP), and networking (Legion_NETWORKS).
#
# We use the same variables as Legion because we want to enable/disable each of
# these features based on how Legion was configured (it doesn't make sense to
# build legate.core's Python bindings if Legion's bindings weren't compiled).
###
legate_core_find_or_configure(PACKAGE Legion)

# If Legion_USE_Python was toggled ON by find_package(Legion), find Python3
if(Legion_USE_Python AND (NOT Python3_FOUND))
  _find_package_Python3()
endif()

if(Legion_NETWORKS)
  rapids_find_package(MPI
    GLOBAL_TARGETS     MPI::MPI_CXX
    BUILD_EXPORT_SET   legate-core-exports
    INSTALL_EXPORT_SET legate-core-exports
    COMPONENTS         CXX
    FIND_ARGS
      REQUIRED
  )
endif()

if(Legion_USE_CUDA)
  # Enable the CUDA language
  enable_language(CUDA)
  # Must come after `enable_language(CUDA)`
  # Use `-isystem <path>` instead of `-isystem=<path>`
  # because the former works with clangd intellisense
  set(CMAKE_INCLUDE_SYSTEM_FLAG_CUDA "-isystem ")
  # Find the CUDAToolkit
  rapids_find_package(
    # Min version of CUDA is 11.8, but we want CMake to prefer the highest version
    # possible. It seems that it only does that if you give it a range...
    #
    # So in 100 years time, if we ever get to CUDA version 100000000, someone must
    # remember to add another 9 below.
    CUDAToolkit 11.8...99999999.99 REQUIRED
    BUILD_EXPORT_SET legate-core-exports
    INSTALL_EXPORT_SET legate-core-exports
  )
  # Find NCCL
  legate_core_find_or_configure(PACKAGE nccl)
endif()

##############################################################################
# - std::span ----------------------------------------------------------------

legate_core_find_or_configure(PACKAGE span)

# find_or_configure_span()

##############################################################################
# - std::mdspan --------------------------------------------------------------

legate_core_find_or_configure(PACKAGE mdspan)

##############################################################################
# - legate.core --------------------------------------------------------------

list(APPEND legate_core_SOURCES
  src/core/comm/comm.cc
  src/core/comm/comm_cpu.cc
  src/core/comm/coll.cc
  src/core/comm/local_comm.cc
  src/core/comm/comm_util.cc
  src/core/cuda/stream_pool.cc
  src/core/data/allocator.cc
  src/core/data/external_allocation.cc
  src/core/data/logical_array.cc
  src/core/data/logical_store.cc
  src/core/data/scalar.cc
  src/core/data/shape.cc
  src/core/data/physical_array.cc
  src/core/data/physical_store.cc
  src/core/data/detail/array_tasks.cc
  src/core/data/detail/attachment.cc
  src/core/data/detail/external_allocation.cc
  src/core/data/detail/logical_array.cc
  src/core/data/detail/logical_region_field.cc
  src/core/data/detail/logical_store.cc
  src/core/data/detail/scalar.cc
  src/core/data/detail/physical_array.cc
  src/core/data/detail/physical_store.cc
  src/core/data/detail/shape.cc
  src/core/data/detail/transform.cc
  src/core/experimental/trace.cc
  src/core/mapping/array.cc
  src/core/mapping/machine.cc
  src/core/mapping/mapping.cc
  src/core/mapping/operation.cc
  src/core/mapping/store.cc
  src/core/mapping/detail/array.cc
  src/core/mapping/detail/base_mapper.cc
  src/core/mapping/detail/core_mapper.cc
  src/core/mapping/detail/instance_manager.cc
  src/core/mapping/detail/machine.cc
  src/core/mapping/detail/mapping.cc
  src/core/mapping/detail/operation.cc
  src/core/mapping/detail/store.cc
  src/core/operation/projection.cc
  src/core/operation/task.cc
  src/core/operation/detail/copy.cc
  src/core/operation/detail/copy_launcher.cc
  src/core/operation/detail/fill.cc
  src/core/operation/detail/fill_launcher.cc
  src/core/operation/detail/gather.cc
  src/core/operation/detail/launcher_arg.cc
  src/core/operation/detail/operation.cc
  src/core/operation/detail/store_projection.cc
  src/core/operation/detail/reduce.cc
  src/core/operation/detail/req_analyzer.cc
  src/core/operation/detail/scatter.cc
  src/core/operation/detail/scatter_gather.cc
  src/core/operation/detail/task.cc
  src/core/operation/detail/task_launcher.cc
  src/core/partitioning/constraint.cc
  src/core/partitioning/partition.cc
  src/core/partitioning/restriction.cc
  src/core/partitioning/detail/constraint.cc
  src/core/partitioning/detail/constraint_solver.cc
  src/core/partitioning/detail/partitioner.cc
  src/core/runtime/library.cc
  src/core/runtime/runtime.cc
  src/core/runtime/scope.cc
  src/core/runtime/detail/communicator_manager.cc
  src/core/runtime/detail/field_manager.cc
  src/core/runtime/detail/library.cc
  src/core/runtime/detail/partition_manager.cc
  src/core/runtime/detail/projection.cc
  src/core/runtime/detail/region_manager.cc
  src/core/runtime/detail/runtime.cc
  src/core/runtime/detail/shard.cc
  src/core/task/registrar.cc
  src/core/task/task.cc
  src/core/task/task_context.cc
  src/core/task/task_info.cc
  src/core/task/variant_options.cc
  src/core/task/detail/return.cc
  src/core/task/detail/return_value.cc
  src/core/task/detail/returned_exception.cc
  src/core/task/detail/returned_cpp_exception.cc
  src/core/task/detail/returned_python_exception.cc
  src/core/task/detail/task_context.cc
  src/core/type/type_info.cc
  src/core/type/detail/type_info.cc
  src/core/utilities/debug.cc
  src/core/utilities/deserializer.cc
  src/core/utilities/machine.cc
  src/core/utilities/linearize.cc
  src/core/utilities/internal_shared_ptr.cc
  src/core/utilities/detail/buffer_builder.cc
  src/core/utilities/detail/tuple.cc
  src/timing/timing.cc
  # stl
  src/core/experimental/stl/detail/clang_tidy_dummy.cpp
)

if(Legion_NETWORKS)
  list(APPEND legate_core_SOURCES
    src/core/comm/mpi_comm.cc)
endif()

if(Legion_USE_OpenMP)
  list(APPEND legate_core_SOURCES
    src/core/data/detail/array_tasks_omp.cc)
endif()

if(Legion_USE_CUDA)
  list(APPEND legate_core_SOURCES
    src/core/comm/comm_nccl.cu
    src/core/data/detail/array_tasks.cu)
  if(CAL_DIR)
    list(APPEND legate_core_SOURCES
      src/core/comm/comm_cal.cu)
  endif()
endif()

add_library(legate_core ${legate_core_SOURCES})
add_library(legate::core ALIAS legate_core)

set(legate_core_CXX_DEFS "")
set(legate_core_CUDA_DEFS "")
set(legate_core_CXX_PRIVATE_OPTIONS "")
set(legate_core_CUDA_PRIVATE_OPTIONS "")
set(legate_core_CXX_PUBLIC_OPTIONS "")
set(legate_core_CUDA_PUBLIC_OPTIONS "")
set(legate_core_LINKER_OPTIONS "")

include(${LEGATE_CORE_DIR}/cmake/Modules/set_cpu_arch_flags.cmake)
set_cpu_arch_flags(legate_core_CXX_PRIVATE_OPTIONS)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND legate_core_CXX_DEFS LEGATE_USE_DEBUG)
  list(APPEND legate_core_CUDA_DEFS LEGATE_USE_DEBUG)
endif()

if(Legion_USE_CUDA)
  list(APPEND legate_core_CXX_DEFS LEGATE_USE_CUDA)
  list(APPEND legate_core_CUDA_DEFS LEGATE_USE_CUDA)

  add_cuda_architecture_defines(legate_core_CUDA_DEFS ARCHS ${Legion_CUDA_ARCH})

  list(APPEND legate_core_CUDA_PRIVATE_OPTIONS -Xfatbin=-compress-all)
  list(APPEND legate_core_CUDA_PRIVATE_OPTIONS --expt-extended-lambda)
  list(APPEND legate_core_CUDA_PRIVATE_OPTIONS --expt-relaxed-constexpr)
  list(APPEND legate_core_CUDA_PRIVATE_OPTIONS -Wno-deprecated-gpu-targets)
endif()

if(Legion_NETWORKS)
  list(APPEND legate_core_CXX_DEFS LEGATE_USE_NETWORK)
  list(APPEND legate_core_CUDA_DEFS LEGATE_USE_NETWORK)
endif()

# Change THRUST_DEVICE_SYSTEM for `.cpp` files
# If we include Thrust in "CUDA mode" in .cc files, that ends up pulling the
# definition of __half from the CUDA toolkit, and Legion defines a custom __half
# when compiling outside of nvcc (because CUDA's __half doesn't define any
# __host__ functions), which causes a conflict.
if(Legion_USE_OpenMP)
  rapids_find_package(OpenMP
    GLOBAL_TARGETS     OpenMP::OpenMP_CXX
    BUILD_EXPORT_SET   legate-core-exports
    INSTALL_EXPORT_SET legate-core-exports
    COMPONENTS         CXX
    FIND_ARGS
      REQUIRED
  )

  list(APPEND legate_core_CXX_DEFS LEGATE_USE_OPENMP)
  list(APPEND legate_core_CUDA_DEFS LEGATE_USE_OPENMP)

  list(APPEND legate_core_CXX_PUBLIC_OPTIONS -UTHRUST_DEVICE_SYSTEM)
  list(APPEND legate_core_CXX_PUBLIC_OPTIONS -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP)
else()
  list(APPEND legate_core_CXX_PUBLIC_OPTIONS -UTHRUST_DEVICE_SYSTEM)
  list(APPEND legate_core_CXX_PUBLIC_OPTIONS -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_CPP)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LEGATE_CORE_PLATFORM_RPATH_ORIGIN "\$ORIGIN")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(LEGATE_CORE_PLATFORM_RPATH_ORIGIN "@loader_path")
else()
  message(
    FATAL_ERROR
    "Unsupported system: ${CMAKE_SYSTEM_NAME}, don't know how to set rpath 'origin' on this platform"
  )
endif ()

set_target_properties(legate_core
           PROPERTIES EXPORT_NAME                         core
                      LIBRARY_OUTPUT_NAME                 lgcore
                      BUILD_RPATH                         "${LEGATE_CORE_PLATFORM_RPATH_ORIGIN}"
                      INSTALL_RPATH                       "${LEGATE_CORE_PLATFORM_RPATH_ORIGIN}"
                      CXX_STANDARD                        ${CMAKE_CXX_STANDARD}
                      CXX_STANDARD_REQUIRED               ON
                      CUDA_STANDARD                       ${CMAKE_CUDA_STANDARD}
                      CUDA_STANDARD_REQUIRED              ON
                      POSITION_INDEPENDENT_CODE           ON
                      INTERFACE_POSITION_INDEPENDENT_CODE ON
                      LIBRARY_OUTPUT_DIRECTORY            lib
                      SOVERSION                           ${legate_core_version})

# export this so that install_info.py can properly locate the versioned and unversioned
# library names
set_property(TARGET legate_core APPEND PROPERTY EXPORT_PROPERTIES LIBRARY_OUTPUT_NAME)

if(Legion_USE_CUDA)
  set_property(TARGET legate_core PROPERTY CUDA_ARCHITECTURES ${Legion_CUDA_ARCH})
endif()

if(Legion_USE_CUDA)
  if(legate_core_STATIC_CUDA_RUNTIME)
    set_target_properties(legate_core PROPERTIES CUDA_RUNTIME_LIBRARY Static)
    # Make sure to export to consumers what runtime we used
    target_link_libraries(legate_core PUBLIC CUDA::cudart_static)
  else()
    set_target_properties(legate_core PROPERTIES CUDA_RUNTIME_LIBRARY Shared)
    # Make sure to export to consumers what runtime we used
    target_link_libraries(legate_core PUBLIC CUDA::cudart)
  endif()
endif()

if(Legion_USE_CUDA AND CAL_DIR)
  message(VERBOSE "legate.core: CAL_DIR ${CAL_DIR}")
  list(APPEND legate_core_CXX_DEFS LEGATE_USE_CAL=1)
  list(APPEND legate_core_CUDA_DEFS LEGATE_USE_CAL=1)
  target_include_directories(legate_core PRIVATE ${CAL_DIR}/include)
  target_link_libraries(legate_core PRIVATE ${CAL_DIR}/lib/libcal.so)
endif()

target_link_libraries(legate_core
  PUBLIC
    Legion::Legion
    CCCL::Thrust
    # See https://cmake.org/cmake/help/latest/module/FindCUDAToolkit.html#nvtx3
    $<TARGET_NAME_IF_EXISTS:$<IF:$<TARGET_EXISTS:CUDA::nvtx3>,CUDA::nvtx3,CUDA::nvToolsExt>>
    $<TARGET_NAME_IF_EXISTS:MPI::MPI_CXX>
    $<TARGET_NAME_IF_EXISTS:OpenMP::OpenMP_CXX>
    $<TARGET_NAME_IF_EXISTS:std::mdspan>
    $<TARGET_NAME_IF_EXISTS:std::span>
  PRIVATE
    $<TARGET_NAME_IF_EXISTS:NCCL::NCCL>
    $<TARGET_NAME_IF_EXISTS:conda_env>
)

target_compile_definitions(legate_core
  PUBLIC "$<$<COMPILE_LANGUAGE:CXX>:${legate_core_CXX_DEFS}>"
         "$<$<COMPILE_LANGUAGE:CUDA>:${legate_core_CUDA_DEFS}>")

##############################################################################
# - Custom User Flags --------------------------------------------------------

macro(legate_core_add_target_compile_option TARGET_NAME OPTION_LANG VIS OPTION_NAME)
  if (NOT ("${${OPTION_NAME}}" MATCHES ".*;.*"))
    # Using this form of separate_arguments() makes sure that quotes are respected when
    # the list is formed. Otherwise stuff like
    #
    # "--compiler-options='-foo -bar -baz'"
    #
    # becomes
    #
    # --compiler-options="'-foo";"-bar";"-baz'"
    #
    # which is obviously not what we wanted
    separate_arguments(${OPTION_NAME} NATIVE_COMMAND "${${OPTION_NAME}}")
  endif()
  if(${OPTION_NAME})
    target_compile_options(${TARGET_NAME} ${VIS} "$<$<COMPILE_LANGUAGE:${OPTION_LANG}>:${${OPTION_NAME}}>")
  endif()
endmacro()

macro(legate_core_add_target_link_option TARGET_NAME VIS OPTION_NAME)
  if (NOT ("${${OPTION_NAME}}" MATCHES ".*;.*"))
    separate_arguments(${OPTION_NAME} NATIVE_COMMAND "${${OPTION_NAME}}")
  endif()
  if(${OPTION_NAME})
    target_link_options(${TARGET_NAME} ${VIS} "${${OPTION_NAME}}")
  endif()
endmacro()

function(check_nvcc_pedantic_flags)
  if(legate_core_SKIP_NVCC_PEDANTIC_CHECK)
    message(VERBOSE "Skipping nvcc pedantic check (explicitly skipped by user)")
    return()
  endif()
  if(NOT (CMAKE_CUDA_COMPILER_ID MATCHES "NVIDIA"))
    message(VERBOSE "Skipping nvcc pedantic check (compiler \"${CMAKE_CUDA_COMPILER_ID}\" is not nvcc)")
    return()
  endif()
  # We want to catch either "-pedantic" or "--compiler-option=-pedantic" or
  # --compiler-options='-pedantic' but we do NOT want to catch -Wformat-pedantic!
  string(REGEX MATCH [=[[ |=|='|="]\-W?pedantic]=] match_var "${legate_core_CUDA_FLAGS}")
  if(match_var)
    message(
      FATAL_ERROR
      "-pedantic (or -Wpedantic) is not supported by nvcc and will lead to "
      "spurious warnings in generated code. Please remove it from your build flags. If "
      "you would like to override this behavior, reconfigure with "
      "-Dlegate_core_SKIP_NVCC_PEDANTIC_CHECK=ON."
    )
  endif()
endfunction()
check_nvcc_pedantic_flags()

include(${LEGATE_CORE_DIR}/cmake/Modules/default_flags.cmake)

legate_core_configure_default_compiler_flags()
legate_core_configure_default_linker_flags()

legate_core_add_target_compile_option(legate_core CXX PRIVATE legate_core_CXX_PRIVATE_OPTIONS)
legate_core_add_target_compile_option(legate_core CUDA PRIVATE legate_core_CUDA_PRIVATE_OPTIONS)

legate_core_add_target_compile_option(legate_core CXX PUBLIC legate_core_CXX_PUBLIC_OPTIONS)
legate_core_add_target_compile_option(legate_core CUDA PUBLIC legate_core_CUDA_PUBLIC_OPTIONS)

legate_core_add_target_compile_option(legate_core CXX PRIVATE legate_core_CXX_FLAGS)
legate_core_add_target_compile_option(legate_core CUDA PRIVATE legate_core_CUDA_FLAGS)

legate_core_add_target_link_option(legate_core PUBLIC legate_core_LINKER_FLAGS)

target_include_directories(legate_core
  PUBLIC
    $<BUILD_INTERFACE:${legate_core_SOURCE_DIR}/src>
  INTERFACE
    $<INSTALL_INTERFACE:include/legate>
)

##############################################################################
# - Doxygen target------------------------------------------------------------

if(legate_core_BUILD_DOCS)
  find_package(Doxygen REQUIRED)
  list(APPEND legate_core_DOC_SOURCES
    # type
    src/core/type/type_info.h
    src/core/type/type_traits.h
    # task
    src/core/task/task.h
    src/core/task/task_context.h
    src/core/task/registrar.h
    src/core/task/variant_options.h
    src/core/task/exception.h
    src/core/cuda/stream_pool.h
    # data
    src/core/data/logical_array.h
    src/core/data/logical_store.h
    src/core/data/physical_array.h
    src/core/data/physical_store.h
    src/core/data/scalar.h
    src/core/data/shape.h
    src/core/data/buffer.h
    src/core/data/external_allocation.h
    src/core/utilities/span.h
    src/core/data/allocator.h
    # runtime
    src/core/runtime/library.h
    src/core/runtime/runtime.h
    src/core/runtime/exception_mode.h
    # operation
    src/core/operation/task.h
    src/core/operation/projection.h
    # partitioning
    src/core/partitioning/constraint.h
    # mapping
    src/core/mapping/machine.h
    src/core/mapping/mapping.h
    src/core/mapping/operation.h
    src/core/mapping/store.h
    # aliases
    src/core/utilities/typedefs.h
    # utilities
    src/core/runtime/scope.h
    src/core/utilities/debug.h
    src/core/utilities/dispatch.h
    src/core/utilities/scope_guard.h
    src/core/utilities/macros.h
    src/core/utilities/compiler.h
    src/timing/timing.h
    # main page
    src/legate.h
  )
  set(DOXYGEN_PROJECT_NAME "Legate")
  set(DOXYGEN_FULL_PATH_NAMES NO)
  set(DOXYGEN_GENERATE_HTML YES)
  set(DOXYGEN_GENERATE_LATEX NO)
  set(DOXYGEN_EXTENSION_MAPPING cu=C++ cuh=C++)
  set(DOXYGEN_HIDE_UNDOC_MEMBERS YES)
  set(DOXYGEN_HIDE_UNDOC_CLASSES YES)
  set(DOXYGEN_USE_MATHJAX YES)
  set(DOXYGEN_MATHJAX_VERSION MathJax_3)
  set(DOXYGEN_STRIP_FROM_INC_PATH ${LEGATE_CORE_DIR}/src)
  set(DOXYGEN_EXAMPLE_PATH tests/cpp)
  set(DOXYGEN_QUIET YES)
  set(DOXYGEN_PREDEFINED DOXYGEN)
  doxygen_add_docs("doxygen_legate" ALL
    ${legate_core_DOC_SOURCES}
    COMMENT "Custom command for building Doxygen docs."
  )
endif()

##############################################################################
# - install targets-----------------------------------------------------------

include(CPack)
include(GNUInstallDirs)

rapids_cmake_install_lib_dir(lib_dir)

install(TARGETS legate_core
        DESTINATION ${lib_dir}
        EXPORT legate-core-exports)

install(
  FILES src/legate.h
        src/legate_defines.h
        src/legate_preamble.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/legate/version_config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate)

install(
  FILES src/core/legate_c.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core)

install(
  FILES src/core/comm/coll.h
        src/core/comm/communicator.h
        src/core/comm/communicator.inl
        src/core/comm/pthread_barrier.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/comm)

install(
  FILES src/core/cuda/cuda.h
        src/core/cuda/stream_pool.h
        src/core/cuda/stream_pool.inl
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/cuda)

install(
  FILES src/core/data/allocator.h
        src/core/data/buffer.h
        src/core/data/external_allocation.h
        src/core/data/external_allocation.inl
        src/core/data/inline_allocation.h
        src/core/data/logical_array.h
        src/core/data/logical_array.inl
        src/core/data/logical_store.h
        src/core/data/logical_store.inl
        src/core/data/physical_array.h
        src/core/data/physical_array.inl
        src/core/data/physical_store.h
        src/core/data/physical_store.inl
        src/core/data/scalar.h
        src/core/data/scalar.inl
        src/core/data/shape.h
        src/core/data/shape.inl
        src/core/data/slice.h
        src/core/data/slice.inl
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/data)

install(
  FILES src/core/experimental/trace.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/experimental)

install(
  FILES src/core/mapping/array.h
        src/core/mapping/array.inl
        src/core/mapping/machine.h
        src/core/mapping/machine.inl
        src/core/mapping/mapping.h
        src/core/mapping/mapping.inl
        src/core/mapping/operation.h
        src/core/mapping/operation.inl
        src/core/mapping/store.h
        src/core/mapping/store.inl
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/mapping)

install(
  FILES src/core/operation/projection.h
        src/core/operation/projection.inl
        src/core/operation/task.h
        src/core/operation/task.inl
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/operation)

install(
  FILES src/core/partitioning/constraint.h
        src/core/partitioning/constraint.inl
        src/core/partitioning/partition.h
        src/core/partitioning/partition.inl
        src/core/partitioning/restriction.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/partitioning)

install(
  FILES src/core/runtime/exception_mode.h
        src/core/runtime/library.h
        src/core/runtime/library.inl
        src/core/runtime/resource.h
        src/core/runtime/runtime.h
        src/core/runtime/runtime.inl
        src/core/runtime/scope.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/runtime)

install(
  FILES src/core/task/exception.h
        src/core/task/exception.inl
        src/core/task/registrar.h
        src/core/task/task.h
        src/core/task/task.inl
        src/core/task/task_context.h
        src/core/task/task_context.inl
        src/core/task/task_info.h
        src/core/task/variant_helper.h
        src/core/task/variant_options.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/task)

install(
  FILES src/core/type/type_info.h
        src/core/type/type_info.inl
        src/core/type/type_traits.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/type)

install(
  FILES src/core/utilities/debug.h
        src/core/utilities/debug.inl
        src/core/utilities/dispatch.h
        src/core/utilities/hash.h
        src/core/utilities/machine.h
        src/core/utilities/memory.h
        src/core/utilities/memory.inl
        src/core/utilities/nvtx_help.h
        src/core/utilities/span.h
        src/core/utilities/span.inl
        src/core/utilities/tuple.h
        src/core/utilities/tuple.inl
        src/core/utilities/typedefs.h
        src/core/utilities/shared_ptr.h
        src/core/utilities/shared_ptr.inl
        src/core/utilities/internal_shared_ptr.h
        src/core/utilities/internal_shared_ptr.inl
        src/core/utilities/cpp_version.h
        src/core/utilities/assert.h
        src/core/utilities/abort.h
        src/core/utilities/scope_guard.h
        src/core/utilities/scope_guard.inl
        src/core/utilities/compiler.h
        src/core/utilities/macros.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/utilities)

install(
  FILES src/core/utilities/detail/compressed_pair.h
        src/core/utilities/detail/shared_ptr_control_block.h
        src/core/utilities/detail/shared_ptr_control_block.inl
        src/core/utilities/detail/type_traits.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/utilities/detail)

# Legate STL headers
install(
	FILES src/core/experimental/stl.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/experimental)

install(
	FILES src/core/experimental/stl/detail/for_each.hpp
		    src/core/experimental/stl/detail/span.hpp
		    src/core/experimental/stl/detail/registrar.hpp
		    src/core/experimental/stl/detail/transform_reduce.hpp
		    src/core/experimental/stl/detail/stlfwd.hpp
		    src/core/experimental/stl/detail/get_logical_store.hpp
		    src/core/experimental/stl/detail/config.hpp
		    src/core/experimental/stl/detail/elementwise.hpp
		    src/core/experimental/stl/detail/functional.hpp
		    src/core/experimental/stl/detail/meta.hpp
		    src/core/experimental/stl/detail/mdspan.hpp
		    src/core/experimental/stl/detail/suffix.hpp
		    src/core/experimental/stl/detail/prefix.hpp
		    src/core/experimental/stl/detail/slice.hpp
		    src/core/experimental/stl/detail/type_traits.hpp
		    src/core/experimental/stl/detail/transform.hpp
		    src/core/experimental/stl/detail/iterator.hpp
		    src/core/experimental/stl/detail/utility.hpp
		    src/core/experimental/stl/detail/store.hpp
		    src/core/experimental/stl/detail/launch_task.hpp
		    src/core/experimental/stl/detail/ranges.hpp
		    src/core/experimental/stl/detail/reduce.hpp
		    src/core/experimental/stl/detail/fill.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/core/experimental/stl/detail)

# Legate timing header
install(
	FILES src/timing/timing.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legate/timing)

install(
  DIRECTORY   ${LEGATE_CORE_DIR}/cmake/Modules
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/legate_core"
  FILES_MATCHING
    PATTERN "*.cmake")

include(${LEGATE_CORE_DIR}/cmake/Modules/debug_symbols.cmake)

legate_core_debug_syms(legate_core INSTALL_DIR ${lib_dir})

##############################################################################
# - install export -----------------------------------------------------------

set(doc_string
        [=[
Provide targets for Legate Core, the Foundation for All Legate Libraries.

Imported Targets:
  - legate::core

]=])

file(READ ${LEGATE_CORE_DIR}/cmake/legate_helper_functions.cmake helper_functions)

# This variable contains the names of the variables that a downstream build wants us to
# "export" back out to them.
#
# Normally this is done transparently (via the "code_string" below, embedded in the
# Findlegate_core.cmake) if the CMakeLists.txt calling this one finds the legate core via
# a find_package() call. But if we are being built as a subdirectory, then we need to
# explicitly set(<the_variable> ... PARENT_SCOPE) in order for downstream to see it...
if(legate_core_SUBDIR_CMAKE_EXPORT_VARS)
  foreach(_var IN LISTS legate_core_SUBDIR_CMAKE_EXPORT_VARS)
    set(${_var} ${${_var}} PARENT_SCOPE)
  endforeach()
endif()

string(JOIN "\n" code_string
[=[
if(NOT TARGET CCCL::Thrust)
  thrust_create_target(CCCL::Thrust FROM_OPTIONS)
endif()
]=]
  "set(Legion_USE_CUDA ${Legion_USE_CUDA})"
  "set(Legion_USE_OpenMP ${Legion_USE_OpenMP})"
  "set(Legion_USE_Python ${Legion_USE_Python})"
  "set(Legion_CUDA_ARCH ${Legion_CUDA_ARCH})"
  "set(Legion_NETWORKS ${Legion_NETWORKS})"
  "set(Legion_BOUNDS_CHECKS ${Legion_BOUNDS_CHECKS})"
  "set(Legion_MAX_DIM ${Legion_MAX_DIM})"
  "set(Legion_MAX_FIELDS ${Legion_MAX_FIELDS})"
[=[
if(Legion_NETWORKS)
  find_package(MPI REQUIRED COMPONENTS CXX)
endif()
]=]
"${helper_functions}"
)

rapids_export(
  INSTALL legate_core
  EXPORT_SET legate-core-exports
  GLOBAL_TARGETS core
  NAMESPACE legate::
  DOCUMENTATION doc_string
  FINAL_CODE_BLOCK code_string
  LANGUAGES ${ENABLED_LANGUAGES}
)

# build export targets
rapids_export(
  BUILD legate_core
  EXPORT_SET legate-core-exports
  GLOBAL_TARGETS core
  NAMESPACE legate::
  DOCUMENTATION doc_string
  FINAL_CODE_BLOCK code_string
  LANGUAGES ${ENABLED_LANGUAES}
)

# Symlink the module directory into the binary dir, so that the helper functions in
# legate_core-config.cmake can be used even if the project is not installed.
message(STATUS "Symlinking cmake module directory into ${CMAKE_CURRENT_BINARY_DIR}")
file(CREATE_LINK
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules
  ${CMAKE_CURRENT_BINARY_DIR}/Modules
  SYMBOLIC
)

set(legate_core_ROOT ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND legate_core_TIDY_SOURCES ${legate_core_SOURCES})

if(legate_core_BUILD_TESTS)
  include(CTest)

  add_subdirectory(${LEGATE_CORE_DIR}/tests/cpp)
endif()

if(legate_core_BUILD_INTEGRATION)
  # TODO:
  # This is broken!
  #
  # CMake Error at build/test-build/legate_core-config.cmake:196 (include):
  # include could not find requested file:
  #
  #   /path/to/legate.core.internal/build/test-build/Modules/include_rapids.cmake
  # Call Stack (most recent call first):
  # cmake/legate_helper_functions.cmake:265 (legate_default_cpp_install)
  # tests/integration/collective/CMakeLists.txt:32 (legate_add_cpp_subdirectory)
  add_subdirectory(${LEGATE_CORE_DIR}/tests/integration)
endif()

if(legate_core_BUILD_EXAMPLES)
  add_subdirectory(${LEGATE_CORE_DIR}/examples)
endif()

include(${LEGATE_CORE_DIR}/cmake/Modules/clang_tidy.cmake)

legate_core_add_tidy_target(SOURCES ${legate_core_TIDY_SOURCES})

# touch these variables so they are not marked as "unused"
set(legate_core_maybe_ignored_variables_ "${legate_core_CMAKE_PRESET_NAME};${CMAKE_BUILD_PARALLEL_LEVEL};")
if(NOT Legion_USE_CUDA)
  list(APPEND legate_core_maybe_ignored_variables_ "${legate_core_CUDA_FLAGS}")
  list(APPEND legate_core_maybe_ignored_variables_ "${CMAKE_CUDA_FLAGS_DEBUG}")
  list(APPEND legate_core_maybe_ignored_variables_ "${CMAKE_CUDA_FLAGS_RELEASE}")
endif()

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
