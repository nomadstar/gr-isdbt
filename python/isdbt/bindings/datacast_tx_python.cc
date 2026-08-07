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
/* BINDTOOL_HEADER_FILE(datacast_tx.h) */
/* BINDTOOL_HEADER_FILE_HASH(77ac638adf66c01706d51b4d5232db74) */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/datacast_tx.h>
// pydoc.h is automatically generated in the build directory
#include <datacast_tx_pydoc.h>

void bind_datacast_tx(py::module& m)
{
    using datacast_tx = ::gr::isdbt::datacast_tx;

    py::class_<datacast_tx, gr::sync_block, gr::block, gr::basic_block,
               std::shared_ptr<datacast_tx>>(m, "datacast_tx", D(datacast_tx))

        .def(py::init(&datacast_tx::make), py::arg("interface") = "tun0",
             py::arg("modulation") = "16QAM", py::arg("fec") = "2/3",
             py::arg("private_pid") = 0x0100,
             py::arg("dst_mac") = "FF:FF:FF:FF:FF:FF",
             py::arg("heartbeat_interval") = 0.1f, py::arg("queue_limit") = 5000,
             py::arg("carousel_repeats") = 3, D(datacast_tx, make))

        ;
}
