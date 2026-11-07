// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_BEAST_TCP_STREAM_HPP
#define WEBCPP_BEAST_TCP_STREAM_HPP

#include <webcpp/beast/config.hpp>

#if WEBCPP_BEAST_HAS_SOCKET_BACKEND
#include WEBCPP_BEAST_BACKEND_HEADER(core/stream_traits.hpp)  // lint-world: stream-layer access
#include WEBCPP_BEAST_BACKEND_HEADER(core/tcp_stream.hpp)  // lint-world: explicit guest TCP backend
#include <webcpp/beast/detail/socket_algorithms.hpp>

namespace webcpp::beast {
/** Finds the bottom layer of a stream stack. */
using ::webcpp::beast_backend::get_lowest_layer;
/**
 Upstream TCP stream with operation deadlines on the selected Asio socket backend.

 Link `/webcpp/beast//sockets` for the selected Asio runtime. Host HTTP does not use this type.
*/
using ::webcpp::beast_backend::tcp_stream;
}  // namespace webcpp::beast
#elif defined(WEBCPP_TARGET_EMSCRIPTEN)
#include <webcpp/beast/browser/tcp_stream.hpp>
#else
#error \
    "The Beast socket backend requires a native or WASI Asio backend; host transports are separate"
#endif

#endif
