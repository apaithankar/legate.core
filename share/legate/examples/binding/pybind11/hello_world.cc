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

#include <legate.h>

#include <iostream>
#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <cstdint>
#include <pybind11/pybind11.h>
#include <type_traits>

namespace hello_world {

class HelloWorld : public legate::LegateTask<HelloWorld> {
 public:
  static constexpr auto TASK_ID = legate::LocalTaskID{0};

  static void cpu_variant(legate::TaskContext);
};

void HelloWorld::cpu_variant(legate::TaskContext) { std::cout << "Hello World!\n"; }

}  // namespace hello_world

namespace {

template <typename T>
constexpr std::underlying_type_t<T> to_underlying(T e)
{
  static_assert(std::is_enum_v<T>);
  return static_cast<std::underlying_type_t<T>>(e);
}

}  // namespace

namespace py = pybind11;

PYBIND11_MODULE(hello_world_pybind11, m)
{
  m.doc() = R"pbdoc(
        Pybind11 example plugin
        -----------------------

        .. currentmodule:: hello_world_pybind11

        .. autosummary::
           :toctree: _generate

           HelloWorld
    )pbdoc";

  py::class_<hello_world::HelloWorld>(m, "HelloWorld")
    .def(py::init<>())
    .def_property_readonly_static(
      "TASK_ID",
      [](py::object /* self */) { return to_underlying(hello_world::HelloWorld::TASK_ID); })
    .def_static("register_variants", [](std::uintptr_t lib_ptr) {
      hello_world::HelloWorld::register_variants(*reinterpret_cast<legate::Library*>(lib_ptr));
    });

#ifdef VERSION_INFO
  m.attr("__version__") = LEGATE_STRINGIZE(VERSION_INFO);
#else
  m.attr("__version__") = "dev";
#endif
}
