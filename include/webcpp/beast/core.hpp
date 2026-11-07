// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_BEAST_CORE_HPP
#define WEBCPP_BEAST_CORE_HPP

#include <webcpp/beast/config.hpp>

#include <webcpp/beast/browser/error.hpp>

#include WEBCPP_BEAST_BACKEND_HEADER(core/bind_handler.hpp)  // lint-world: binds handler arguments only
#include WEBCPP_BEAST_BACKEND_HEADER(core/buffers_prefix.hpp)  // lint-world: buffer-sequence views only
#include WEBCPP_BEAST_BACKEND_HEADER(core/buffers_to_string.hpp)  // lint-world: copies memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/error.hpp)              // lint-world: error codes only
#include WEBCPP_BEAST_BACKEND_HEADER(core/file_base.hpp)  // lint-world: file access mode enumeration
#include WEBCPP_BEAST_BACKEND_HEADER(core/flat_buffer.hpp)  // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/flat_static_buffer.hpp)  // lint-world: fixed-capacity memory buffers
#include WEBCPP_BEAST_BACKEND_HEADER(core/make_printable.hpp)  // lint-world: buffer output formatting only
#include WEBCPP_BEAST_BACKEND_HEADER(core/multi_buffer.hpp)  // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/ostream.hpp)  // lint-world: writes into memory buffers
#include WEBCPP_BEAST_BACKEND_HEADER(core/static_buffer.hpp)  // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/string.hpp)         // lint-world: string comparison only

namespace webcpp::beast {
/** Upstream contiguous dynamic buffer with an application-supplied allocator. */
using ::webcpp::beast_backend::basic_flat_buffer;
/** Upstream segmented dynamic buffer with an application-supplied allocator. */
using ::webcpp::beast_backend::basic_multi_buffer;
/** Binds leading arguments to a completion handler. */
using ::webcpp::beast_backend::bind_front_handler;
/** Returns a view of the first bytes of a buffer sequence. */
using ::webcpp::beast_backend::buffers_prefix;
/** Copies a buffer sequence to a string. */
using ::webcpp::beast_backend::buffers_to_string;
/** Upstream common stream errors, including timeout. */
using ::webcpp::beast_backend::error;
/** The error-code type used by Beast operations. */
using ::webcpp::beast_backend::error_code;
/** Upstream generic error conditions. */
namespace errc = ::webcpp::beast_backend::errc;
/** Upstream file-opening modes for explicit file bodies. */
using ::webcpp::beast_backend::file_mode;
/** Upstream contiguous dynamic buffer. */
using ::webcpp::beast_backend::flat_buffer;
/** Upstream contiguous buffer with fixed capacity. */
using ::webcpp::beast_backend::flat_static_buffer;
/** Compares strings with HTTP's ASCII case-insensitive rules. */
using ::webcpp::beast_backend::iequals;
/** Formats a buffer sequence for diagnostic output. */
using ::webcpp::beast_backend::make_printable;
/** Upstream segmented dynamic buffer. */
using ::webcpp::beast_backend::multi_buffer;
/** Creates an output stream that appends to a dynamic buffer. */
using ::webcpp::beast_backend::ostream;
/** Upstream fixed-capacity buffer. */
using ::webcpp::beast_backend::static_buffer;
/** Upstream non-owning string view. */
using ::webcpp::beast_backend::string_view;
/** Upstream exception type for explicitly throwing application code. */
using ::webcpp::beast_backend::system_error;
}  // namespace webcpp::beast

#endif
