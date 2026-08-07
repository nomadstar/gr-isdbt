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
/* BINDTOOL_HEADER_FILE(mer_snr_estimator.h) */
/* BINDTOOL_HEADER_FILE_HASH(bc83ed0dcbfb887dd5ccbb01b757efd7) */

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <gnuradio/isdbt/mer_snr_estimator.h>
// pydoc.h is automatically generated in the build directory
#include <mer_snr_estimator_pydoc.h>

void bind_mer_snr_estimator(py::module& m)
{
    using mer_snr_estimator = ::gr::isdbt::mer_snr_estimator;

    py::class_<mer_snr_estimator, gr::sync_block, gr::block, gr::basic_block,
               std::shared_ptr<mer_snr_estimator>>(m, "mer_snr_estimator",
                                                     D(mer_snr_estimator))

        .def(py::init(&mer_snr_estimator::make), py::arg("constellation") = "16QAM",
             D(mer_snr_estimator, make))

        ;
}
