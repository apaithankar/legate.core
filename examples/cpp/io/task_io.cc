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

#include "task_io.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace task {

namespace legateio {

Legion::Logger logger(library_name);

void register_tasks()
{
  auto runtime = legate::Runtime::get_runtime();
  auto library = runtime->create_library(library_name);
  ReadEvenTilesTask::register_variants(library);
  ReadFileTask::register_variants(library);
  ReadUnevenTilesTask::register_variants(library);
  WriteEvenTilesTask::register_variants(library);
  WriteFileTask::register_variants(library);
  WriteUnevenTilesTask::register_variants(library);
}

namespace utils {

struct write_util_fn {
  template <legate::Type::Code CODE, std::int32_t DIM>
  void operator()(const legate::PhysicalStore& store, const fs::path& path)
  {
    using VAL = legate::legate_type_of<CODE>;

    auto shape = store.shape<DIM>();
    auto empty = shape.empty();
    auto extents =
      empty ? legate::Point<DIM>::ZEROES() : shape.hi - shape.lo + legate::Point<DIM>::ONES();

    logger.debug() << "Write a sub-array " << shape << " to " << path;

    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
    // Each file for a chunk starts with the extents
    for (std::int32_t idx = 0; idx < DIM; ++idx) {
      out.write(reinterpret_cast<const char*>(&extents[idx]), sizeof(legate::coord_t));
    }

    if (empty) {
      return;
    }
    auto acc = store.read_accessor<VAL, DIM>();
    // The iteration order here should be consistent with that in the reader task, otherwise
    // the read data can be transposed.
    for (legate::PointInRectIterator<DIM> it(shape, false /*fortran_order*/); it.valid(); ++it) {
      auto ptr = acc.ptr(*it);
      out.write(reinterpret_cast<const char*>(ptr), sizeof(VAL));
    }
  }
};

std::filesystem::path get_unique_path_for_task_index(const legate::TaskContext context,
                                                     std::int32_t ndim,
                                                     const std::string& dirname)
{
  auto task_index = context.get_task_index();
  // If this was a single task, we use (0, ..., 0) for the task index
  if (context.is_single_task()) {
    task_index     = legate::DomainPoint();
    task_index.dim = ndim;
  }

  std::stringstream ss;
  for (std::int32_t idx = 0; idx < task_index.dim; ++idx) {
    if (idx != 0) {
      ss << ".";
    }
    ss << task_index[idx];
  }
  auto filename = ss.str();

  return fs::path(dirname) / filename;
}

void write_to_file(legate::TaskContext task_context,
                   const std::string& dirname,
                   const legate::PhysicalStore& store)
{
  auto path = get_unique_path_for_task_index(task_context, store.dim(), dirname);
  // double_dispatch converts the first two arguments to non-type template arguments
  legate::double_dispatch(store.dim(), store.code(), write_util_fn{}, store, path);
}

}  // namespace utils

struct read_even_fn {
  template <legate::Type::Code CODE, std::int32_t DIM>
  void operator()(legate::PhysicalStore& output, const fs::path& path)
  {
    using VAL = legate::legate_type_of<CODE>;

    legate::Rect<DIM> shape = output.shape<DIM>();

    if (shape.empty()) {
      return;
    }

    std::ifstream in(path, std::ios::binary | std::ios::in);

    legate::Point<DIM> extents;
    for (std::int32_t idx = 0; idx < DIM; ++idx) {
      in.read(reinterpret_cast<char*>(&extents[idx]), sizeof(legate::coord_t));
    }

    // Since the shape is already fixed on the Python side, the sub-store's extents should be the
    // same as what's stored in the file
    LEGATE_CHECK(shape.hi - shape.lo + legate::Point<DIM>::ONES() == extents);

    logger.debug() << "Read a sub-array of rect " << shape << " from " << path;

    auto acc = output.write_accessor<VAL, DIM>();
    for (legate::PointInRectIterator<DIM> it(shape, false /*fortran_order*/); it.valid(); ++it) {
      auto ptr = acc.ptr(*it);
      in.read(reinterpret_cast<char*>(ptr), sizeof(VAL));
    }
  }
};

/*static*/ void ReadEvenTilesTask::cpu_variant(legate::TaskContext context)
{
  auto dirname = context.scalar(0).value<std::string>();
  auto output  = context.output(0).data();

  auto path = utils::get_unique_path_for_task_index(context, output.dim(), dirname);
  // double_dispatch converts the first two arguments to non-type template arguments
  legate::double_dispatch(output.dim(), output.code(), read_even_fn{}, output, path);
}

struct read_fn {
  template <legate::Type::Code CODE>
  void operator()(legate::PhysicalStore& output,
                  const std::string& filename,
                  std::int64_t my_id,
                  std::int64_t num_readers)
  {
    using VAL = legate::legate_type_of<CODE>;

    std::int64_t code;
    std::size_t size;

    // All reader tasks need to read the header to figure out the total number of elements
    std::ifstream in(filename, std::ios::binary | std::ios::in);
    in.read(reinterpret_cast<char*>(&code), sizeof(int64_t));
    in.read(reinterpret_cast<char*>(&size), sizeof(size_t));

    if (static_cast<legate::Type::Code>(code) != CODE) {
      LEGATE_ABORT("Type mismatch: " << CODE << " != " << code);
    }

    // Compute the absolute offsets to the section that this reader task
    // is supposed to read from the file
    std::int64_t my_lo = my_id * size / num_readers;
    std::int64_t my_hi = std::min((my_id + 1) * size / num_readers, size);

    // Then compute the extent for the output and create the output buffer to populate
    std::int64_t my_ext = my_hi - my_lo;
    auto buf            = output.create_output_buffer<VAL, 1>(legate::Point<1>(my_ext));

    // Skip to the right offset where the data assigned to this reader task actually starts
    if (my_lo != 0) {
      in.seekg(my_lo * sizeof(VAL), std::ios_base::cur);
    }
    for (std::int64_t idx = 0; idx < my_ext; ++idx) {
      auto ptr = buf.ptr(legate::Point<1>(idx));
      in.read(reinterpret_cast<char*>(ptr), sizeof(VAL));
    }

    // Finally, bind the output buffer to the store
    //
    // Some minor details about unbound stores:
    //
    // 1) Though this example binds the buffer after it's populated, it's not strictly necessary.
    //    In fact, the create_output_buffer call takes an optional boolean argument that binds
    //    the created buffer immediately.
    // 2) The bind_data call takes the extents of data as an argument, just in case the buffer
    //    is actually bigger than extents of the actual data.
    //
    output.bind_data(buf, legate::Point<1>(my_ext));
  }
};

/*static*/ void ReadFileTask::cpu_variant(legate::TaskContext context)
{
  auto filename = context.scalar(0).value<std::string>();
  auto output   = context.output(0).data();

  // The task context contains metadata about the launch so each reader task can figure out
  // which part of the file it needs to read into the output.
  std::int64_t my_id = context.is_single_task() ? 0 : context.get_task_index()[0];
  std::int64_t num_readers =
    context.is_single_task() ? 1 : context.get_launch_domain().get_volume();
  logger.debug() << "Read " << filename << " (" << my_id + 1 << "/" << num_readers << ")";

  // type_dispatch converts the first argument to a non-type template argument
  legate::type_dispatch(output.code(), read_fn{}, output, filename, my_id, num_readers);
}

struct read_uneven_fn {
  template <legate::Type::Code CODE, std::int32_t DIM>
  void operator()(legate::PhysicalStore& output, const fs::path& path)
  {
    using VAL = legate::legate_type_of<CODE>;

    std::ifstream in(path, std::ios::binary | std::ios::in);

    // Read the header of each file to extract the extents
    legate::Point<DIM> extents;
    for (std::int32_t idx = 0; idx < DIM; ++idx) {
      in.read(reinterpret_cast<char*>(&extents[idx]), sizeof(legate::coord_t));
    }

    logger.debug() << "Read a sub-array of extents " << extents << " from " << path;

    // Use the extents to create an output buffer
    auto buf = output.create_output_buffer<VAL, DIM>(extents);
    legate::Rect<DIM> shape(legate::Point<DIM>::ZEROES(), extents - legate::Point<DIM>::ONES());
    if (!shape.empty()) {
      // Read the file data. The iteration order here should be the same as in the writer task
      for (legate::PointInRectIterator<DIM> it(shape, false /*fortran_order*/); it.valid(); ++it) {
        auto ptr = buf.ptr(*it);
        in.read(reinterpret_cast<char*>(ptr), sizeof(VAL));
      }
    }

    // Finally, bind the output buffer to the store
    output.bind_data(buf, extents);
  }
};

/*static*/ void ReadUnevenTilesTask::cpu_variant(legate::TaskContext context)
{
  auto dirname = context.scalar(0).value<std::string>();
  auto output  = context.output(0).data();

  auto path = utils::get_unique_path_for_task_index(context, output.dim(), dirname);
  // double_dispatch converts the first two arguments to non-type template arguments
  legate::double_dispatch(output.dim(), output.code(), read_uneven_fn{}, output, path);
}

void write_header(std::ofstream& out,
                  legate::Type::Code type_code,
                  const legate::Span<const std::int32_t>& shape,
                  const legate::Span<const std::int32_t>& tile_shape)
{
  LEGATE_CHECK(shape.size() == tile_shape.size());
  std::int32_t dim = shape.size();
  // Dump the type code, the array's shape and the tile shape to the header
  out.write(reinterpret_cast<const char*>(&type_code), sizeof(int32_t));
  out.write(reinterpret_cast<const char*>(&dim), sizeof(int32_t));
  for (auto& v : shape) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
  }
  for (auto& v : tile_shape) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
  }
}

/*static*/ void WriteEvenTilesTask::cpu_variant(legate::TaskContext context)
{
  auto dirname                                = context.scalar(0).value<std::string>();
  legate::Span<const std::int32_t> shape      = context.scalar(1).values<std::int32_t>();
  legate::Span<const std::int32_t> tile_shape = context.scalar(2).values<std::int32_t>();
  auto input                                  = context.input(0).data();

  auto launch_domain = context.get_launch_domain();
  auto task_index    = context.get_task_index();
  auto is_first_task = context.is_single_task() || task_index == launch_domain.lo();

  if (is_first_task) {
    auto header = fs::path(dirname) / ".header";
    logger.debug() << "Write to " << header;
    std::ofstream out(header, std::ios::binary | std::ios::out | std::ios::trunc);
    write_header(out, input.code(), shape, tile_shape);
  }

  utils::write_to_file(context, dirname, input);
}

struct write_fn {
  template <legate::Type::Code CODE>
  void operator()(const legate::PhysicalStore& input, const std::string& filename)
  {
    using VAL = legate::legate_type_of<CODE>;

    auto shape       = input.shape<1>();
    auto code        = input.code<std::int64_t>();
    std::size_t size = shape.volume();

    // Store the type code and the number of elements in the array at the beginning of the file
    std::ofstream out(filename, std::ios::binary | std::ios::out | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&code), sizeof(int64_t));
    out.write(reinterpret_cast<const char*>(&size), sizeof(size_t));

    auto acc = input.read_accessor<VAL, 1>();
    for (legate::PointInRectIterator<1> it(shape); it.valid(); ++it) {
      auto ptr = acc.ptr(*it);
      out.write(reinterpret_cast<const char*>(ptr), sizeof(VAL));
    }
  }
};

/*statis*/ void WriteFileTask::cpu_variant(legate::TaskContext context)
{
  auto filename = context.scalar(0).value<std::string>();
  auto input    = context.input(0).data();
  logger.debug() << "Write to " << filename;

  legate::type_dispatch(input.code(), write_fn{}, input, filename);
}

struct header_write_fn {
  template <std::int32_t DIM>
  void operator()(std::ofstream& out,
                  const legate::Domain& launch_domain,
                  legate::Type::Code type_code)
  {
    legate::Rect<DIM> rect(launch_domain);
    auto extents = rect.hi - rect.lo + legate::Point<DIM>::ONES();

    // The header contains the type code and the launch shape
    out.write(reinterpret_cast<const char*>(&type_code), sizeof(int32_t));
    out.write(reinterpret_cast<const char*>(&launch_domain.dim), sizeof(int32_t));
    for (std::int32_t idx = 0; idx < DIM; ++idx) {
      out.write(reinterpret_cast<const char*>(&extents[idx]), sizeof(legate::coord_t));
    }
  }
};

/*statis*/ void WriteUnevenTilesTask::cpu_variant(legate::TaskContext context)
{
  auto dirname = context.scalar(0).value<std::string>();
  auto input   = context.input(0).data();

  auto launch_domain = context.get_launch_domain();
  auto task_index    = context.get_task_index();
  auto is_first_task = context.is_single_task() || task_index == launch_domain.lo();

  // Only the first task needs to dump the header
  if (is_first_task) {
    // When the task is a single task, we create a launch domain of volume 1
    if (context.is_single_task()) {
      launch_domain     = legate::Domain();
      launch_domain.dim = input.dim();
    }

    auto header = fs::path(dirname) / ".header";
    logger.debug() << "Write to " << header;
    std::ofstream out(header, std::ios::binary | std::ios::out | std::ios::trunc);
    legate::dim_dispatch(launch_domain.dim, header_write_fn{}, out, launch_domain, input.code());
  }

  utils::write_to_file(context, dirname, input);
}

}  // namespace legateio

}  // namespace task
