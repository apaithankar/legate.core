#=============================================================================
# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.
#=============================================================================

list(APPEND CMAKE_MESSAGE_CONTEXT "options")

option(legate_BUILD_TESTS "Whether to build the C++ tests")
option(legate_BUILD_EXAMPLES "Whether to build the C++/python examples")
option(BUILD_SHARED_LIBS "Build legate.shared libraries" ON)

function(set_or_default var_name var_env)
  list(LENGTH ARGN num_extra_args)
  if(num_extra_args GREATER 0)
    list(GET ARGN 0 var_default)
  endif()
  if(DEFINED ${var_name})
    message(VERBOSE "legate: ${var_name}=${${var_name}}")
  elseif(DEFINED ENV{${var_env}})
    set(${var_name} $ENV{${var_env}} PARENT_SCOPE)
    message(VERBOSE "legate: ${var_name}=$ENV{${var_env}} (from envvar '${var_env}')")
  elseif(DEFINED var_default)
    set(${var_name} ${var_default} PARENT_SCOPE)
    message(VERBOSE "legate: ${var_name}=${var_default} (from default value)")
  else()
    message(VERBOSE "legate: not setting ${var_name}")
  endif()
endfunction()

# Initialize these vars from the CLI, then fallback to an envvar or a default value.
set_or_default(Legion_SPY USE_SPY OFF)
set_or_default(Legion_USE_LLVM USE_LLVM OFF)
set_or_default(Legion_USE_CUDA USE_CUDA OFF)
set_or_default(Legion_USE_HDF5 USE_HDF OFF)
set_or_default(Legion_NETWORKS NETWORKS "")
set_or_default(Legion_USE_OpenMP USE_OPENMP OFF)
set_or_default(Legion_BOUNDS_CHECKS CHECK_BOUNDS OFF)
set_or_default(legate_SKIP_NVCC_PEDANTIC_CHECK legate_SKIP_NVCC_PEDANTIC_CHECK OFF)
set_or_default(legate_ENABLE_SANITIZERS legate_ENABLE_SANITIZERS OFF)
set_or_default(legate_IGNORE_INSTALLED_PACKAGES legate_IGNORE_INSTALLED_PACKAGES OFF)

option(Legion_SPY "Enable detailed logging for Legion Spy" OFF)
option(Legion_USE_LLVM "Use LLVM JIT operations" OFF)
option(Legion_USE_HDF5 "Enable support for HDF5" OFF)
option(Legion_USE_CUDA "Enable Legion support for the CUDA runtime" OFF)
option(Legion_NETWORKS "Networking backends to use (semicolon-separated)" "")
option(Legion_USE_OpenMP "Use OpenMP" OFF)
option(Legion_USE_Python "Use Python" OFF)
option(Legion_BOUNDS_CHECKS "Enable bounds checking in Legion accessors" OFF)
option(legate_SKIP_NVCC_PEDANTIC_CHECK
       "Skip checking for -pedantic or -Wpedantic compiler flags for NVCC" OFF)
option(legate_ENABLE_SANITIZERS "Enable sanitizer support for legate" OFF)
option(legate_IGNORE_INSTALLED_PACKAGES
       "When deciding to search for or download third-party packages, never search and always download"
       OFF)

if("${Legion_NETWORKS}" MATCHES ".*gasnet(1|ex).*")
  set_or_default(GASNet_ROOT_DIR GASNET)
  set_or_default(GASNet_CONDUIT CONDUIT "mpi")

  if(NOT GASNet_ROOT_DIR)
    option(Legion_EMBED_GASNet "Embed a custom GASNet build into Legion" ON)
  endif()
endif()

set_or_default(Legion_MAX_DIM LEGION_MAX_DIM 4)

# Check the max dimensions
if((Legion_MAX_DIM LESS 1) OR (Legion_MAX_DIM GREATER 9))
  message(FATAL_ERROR "The maximum number of Legate dimensions must be between 1 and 9 inclusive"
  )
endif()

set_or_default(Legion_MAX_FIELDS LEGION_MAX_FIELDS 256)

# Check that max fields is between 32 and 4096 and is a power of 2
if(NOT Legion_MAX_FIELDS MATCHES "^(32|64|128|256|512|1024|2048|4096)$")
  message(FATAL_ERROR "The maximum number of Legate fields must be a power of 2 between 32 and 4096 inclusive"
  )
endif()

# We never want local fields
set(Legion_DEFAULT_LOCAL_FIELDS 0)

option(legate_STATIC_CUDA_RUNTIME "Statically link the cuda runtime library" OFF)
option(legate_EXCLUDE_LEGION_FROM_ALL "Exclude Legion targets from legate's 'all' target"
       OFF)
option(legate_BUILD_DOCS "Build doxygen docs" OFF)

set_or_default(NCCL_DIR NCCL_PATH)
set_or_default(CUDA_TOOLKIT_ROOT_DIR CUDA)
set_or_default(Legion_CUDA_ARCH GPU_ARCH all-major)
set_or_default(Legion_HIJACK_CUDART USE_CUDART_HIJACK OFF)

include(CMakeDependentOption)
cmake_dependent_option(Legion_HIJACK_CUDART
                       "Allow Legion to hijack and rewrite application calls into the CUDA runtime"
                       ON
                       "Legion_USE_CUDA;Legion_HIJACK_CUDART"
                       OFF)
# This needs to be added as an option to force values to be visible in Legion build
option(Legion_HIJACK_CUDART "Replace default CUDA runtime with the Realm version" OFF)

if(Legion_HIJACK_CUDART)
  message(WARNING [=[
#####################################################################
Warning: Realm's CUDA runtime hijack is incompatible with NCCL.
Please note that your code will crash catastrophically as soon as it
calls into NCCL either directly or through some other Legate library.
#####################################################################
]=])
endif()

if(BUILD_SHARED_LIBS)
  if(Legion_HIJACK_CUDART)
    # Statically link CUDA if HIJACK_CUDART is set
    set(Legion_CUDA_DYNAMIC_LOAD OFF)
    set(CUDA_USE_STATIC_CUDA_RUNTIME ON)
  elseif(NOT DEFINED Legion_CUDA_DYNAMIC_LOAD)
    # If HIJACK_CUDART isn't set and BUILD_SHARED_LIBS is true, default
    # Legion_CUDA_DYNAMIC_LOAD to true
    set(Legion_CUDA_DYNAMIC_LOAD ON)
    set(CUDA_USE_STATIC_CUDA_RUNTIME OFF)
  endif()
elseif(NOT DEFINED Legion_CUDA_DYNAMIC_LOAD)
  # If BUILD_SHARED_LIBS is false, default Legion_CUDA_DYNAMIC_LOAD to false also
  set(Legion_CUDA_DYNAMIC_LOAD OFF)
  set(CUDA_USE_STATIC_CUDA_RUNTIME ON)
endif()

set(legate_CXX_FLAGS "" CACHE STRING "C++ flags for legate")
set(legate_CUDA_FLAGS "" CACHE STRING "CUDA flags for legate")
set(legate_LINKER_FLAGS "" CACHE STRING "Linker flags for legate")

# there must be some way to automate creating these for all dependent packages...
set(Legion_CXX_FLAGS "" CACHE STRING "C++ flags for Legion")
set(Legion_CUDA_FLAGS "" CACHE STRING "CUDA flags for Legion")
set(Legion_LINKER_FLAGS "" CACHE STRING "Linker flags for Legion")

list(POP_BACK CMAKE_MESSAGE_CONTEXT)
