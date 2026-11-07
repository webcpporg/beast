// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_BEAST_DETAIL_SOCKET_ALGORITHMS_HPP
#define WEBCPP_BEAST_DETAIL_SOCKET_ALGORITHMS_HPP

#include <webcpp/beast/config.hpp>

#if WEBCPP_BEAST_HAS_SOCKET_BACKEND
#include WEBCPP_BEAST_BACKEND_HEADER(core/async_base.hpp)  // lint-world: composed operation support
#include WEBCPP_BEAST_BACKEND_HEADER(core/basic_stream.hpp)  // lint-world: explicitly selected socket backend
#include WEBCPP_BEAST_BACKEND_HEADER(core/buffers_generator.hpp)  // lint-world: explicitly selected stream writes
#include WEBCPP_BEAST_BACKEND_HEADER(core/detect_ssl.hpp)  // lint-world: TLS detection over a byte stream
#include WEBCPP_BEAST_BACKEND_HEADER(core/rate_policy.hpp)  // lint-world: stream rate policies
#include WEBCPP_BEAST_BACKEND_HEADER(core/role.hpp)         // lint-world: protocol role enumeration
#include WEBCPP_BEAST_BACKEND_HEADER(core/stream_traits.hpp)  // lint-world: stream concepts
#include WEBCPP_BEAST_BACKEND_HEADER(http/message_generator.hpp)  // lint-world: explicit socket message writing

namespace webcpp::beast {
/** Base for an allocator-aware composed asynchronous operation. */
using ::webcpp::beast_backend::async_base;
/** Detects a TLS ClientHello without consuming it. */
using ::webcpp::beast_backend::async_detect_ssl;
/** Writes a buffer generator to an asynchronous byte stream. */
using ::webcpp::beast_backend::async_write;
/** Upstream socket stream with deadlines and rate policies. */
using ::webcpp::beast_backend::basic_stream;
/** Upstream executor type customization. */
using ::webcpp::beast_backend::executor_type;
/** Tests whether a type satisfies Beast's asynchronous stream requirements. */
using ::webcpp::beast_backend::is_async_stream;
/** Identifies a protocol's client or server role. */
using ::webcpp::beast_backend::role_type;
/** Socket rate policy that does not limit throughput. */
using ::webcpp::beast_backend::unlimited_rate_policy;
/** Writes a buffer generator to a synchronous byte stream. */
using ::webcpp::beast_backend::write;
}  // namespace webcpp::beast

namespace webcpp::beast::http {
/** Type-erased message serializer for the explicitly selected socket backend. */
using ::webcpp::beast_backend::http::message_generator;
}  // namespace webcpp::beast::http
#endif

#endif
