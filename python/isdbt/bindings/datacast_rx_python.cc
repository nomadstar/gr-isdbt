/*
 * Copyright 2025 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/* BINDTOOL_GEN_AUTOMATIC(0) */
/* BINDTOOL_USE_PYGCCXML(0) */
/* BINDTOOL_HEADER_FILE(datacast_rx.h) */
/* BINDTOOL_HEADER_FILE_HASH(e044297e78959229a91f0acba05fdbab) */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/datacast_rx.h>
// pydoc.h is automatically generated in the build directory
#include <datacast_rx_pydoc.h>

void bind_datacast_rx(py::module& m)
{
    using datacast_rx = ::gr::isdbt::datacast_rx;

    py::class_<datacast_rx, gr::block, gr::basic_block, std::shared_ptr<datacast_rx>>(
        m, "datacast_rx", D(datacast_rx))

        .def(py::init(&datacast_rx::make), py::arg("target_pid") = 0x0100,
             py::arg("port_detection_window") = 20, py::arg("reorder_window") = 50000,
             py::arg("output_interface") = "", D(datacast_rx, make))

        ;
}
