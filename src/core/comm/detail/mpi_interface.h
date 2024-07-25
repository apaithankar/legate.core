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

#pragma once

#include "core/utilities/abort.h"
#include "core/utilities/macros.h"

#include "../share/legate/lib/mpi_wrapper_types.h"

#include <cstddef>

namespace legate::detail::comm::mpi::detail {

class MPIInterface {
 public:
  using MPI_Comm     = Legate_MPI_Comm;
  using MPI_Datatype = Legate_MPI_Datatype;
  using MPI_Aint     = Legate_MPI_Aint;
  using MPI_Status   = Legate_MPI_Status;

  // NOLINTBEGIN(readability-identifier-naming)
  static MPI_Comm MPI_COMM_WORLD();
  static int MPI_THREAD_MULTIPLE();
  static int MPI_TAG_UB();
  static int MPI_CONGRUENT();
  static int MPI_SUCCESS();

  static MPI_Datatype MPI_INT8_T();
  static MPI_Datatype MPI_UINT8_T();
  static MPI_Datatype MPI_CHAR();
  static MPI_Datatype MPI_INT();
  static MPI_Datatype MPI_BYTE();
  static MPI_Datatype MPI_INT32_T();
  static MPI_Datatype MPI_UINT32_T();
  static MPI_Datatype MPI_INT64_T();
  static MPI_Datatype MPI_UINT64_T();
  static MPI_Datatype MPI_FLOAT();
  static MPI_Datatype MPI_DOUBLE();
  // NOLINTEND(readability-identifier-naming)

  static int mpi_init_thread(int* argc, char*** argv, int required, int* provided);
  static int mpi_finalize();
  static int mpi_abort(MPI_Comm comm, int error_code);

  static int mpi_initialized(int* init);
  static int mpi_finalized(int* finalized);

  static int mpi_comm_dup(MPI_Comm comm, MPI_Comm* dup);
  static int mpi_comm_rank(MPI_Comm comm, int* rank);
  static int mpi_comm_size(MPI_Comm comm, int* size);
  static int mpi_comm_compare(MPI_Comm comm1, MPI_Comm comm2, int* result);
  static int mpi_comm_get_attr(MPI_Comm, int comm_keyval, void* attribute_val, int* flag);
  static int mpi_comm_free(MPI_Comm* comm);

  static int mpi_type_get_extent(MPI_Datatype type, MPI_Aint* lb, MPI_Aint* extent);

  static int mpi_query_thread(int* provided);

  static int mpi_bcast(void* buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
  static int mpi_send(
    const void* buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
  static int mpi_recv(void* buf,
                      int count,
                      MPI_Datatype datatype,
                      int source,
                      int tag,
                      MPI_Comm comm,
                      MPI_Status* status);
  static int mpi_sendrecv(const void* sendbuf,
                          int sendcount,
                          MPI_Datatype sendtype,
                          int dest,
                          int sendtag,
                          void* recvbuf,
                          int recvcount,
                          MPI_Datatype recvtype,
                          int source,
                          int recvtag,
                          MPI_Comm comm,
                          MPI_Status* status);
};

}  // namespace legate::detail::comm::mpi::detail

#define LEGATE_CHECK_MPI(...)                                                                     \
  do {                                                                                            \
    const int lgcore_check_mpi_result_ = __VA_ARGS__;                                             \
    if (LEGATE_UNLIKELY(lgcore_check_mpi_result_ !=                                               \
                        legate::detail::comm::mpi::detail::MPIInterface::MPI_SUCCESS())) {        \
      LEGATE_ABORT("Internal MPI failure with error code "                                        \
                   << lgcore_check_mpi_result_ << " in " << __FILE__ << ":" << __LINE__ << " in " \
                   << __func__ << "(): " << LEGATE_STRINGIZE(__VA_ARGS__));                       \
    }                                                                                             \
  } while (0)
