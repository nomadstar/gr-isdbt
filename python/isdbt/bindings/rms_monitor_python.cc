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
/* BINDTOOL_HEADER_FILE(rms_monitor.h) */
/* BINDTOOL_HEADER_FILE_HASH(ac910db87513207886e4fa850f930b6b) */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/rms_monitor.h>
// pydoc.h is automatically generated in the build directory
#include <rms_monitor_pydoc.h>

void bind_rms_monitor(py::module& m)
{
    using rms_monitor = ::gr::isdbt::rms_monitor;

    py::class_<rms_monitor, gr::sync_block, gr::block, gr::basic_block,
               std::shared_ptr<rms_monitor>>(m, "rms_monitor", D(rms_monitor))

        .def(py::init(&rms_monitor::make), D(rms_monitor, make))

        ;
}
