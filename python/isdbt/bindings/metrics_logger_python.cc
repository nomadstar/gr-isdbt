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
/* BINDTOOL_HEADER_FILE(metrics_logger.h) */
/* BINDTOOL_HEADER_FILE_HASH(4e4d7f6c586ab3e1594b0257a15adb1a) */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/metrics_logger.h>
// pydoc.h is automatically generated in the build directory
#include <metrics_logger_pydoc.h>

void bind_metrics_logger(py::module& m)
{
    using metrics_logger = ::gr::isdbt::metrics_logger;

    py::class_<metrics_logger, gr::sync_block, gr::block, gr::basic_block,
               std::shared_ptr<metrics_logger>>(m, "metrics_logger", D(metrics_logger))

        .def(py::init(&metrics_logger::make),
             py::arg("filename") = "resultados_isdbt.csv",
             py::arg("print_interval") = 1.0f, D(metrics_logger, make))

        ;
}
