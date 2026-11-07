// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_BEAST_HTTP_HPP
#define WEBCPP_BEAST_HTTP_HPP

#include <webcpp/beast/config.hpp>

#include WEBCPP_BEAST_BACKEND_HEADER(http/buffer_body.hpp)  // lint-world: caller-owned memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(http/dynamic_body.hpp)  // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(http/empty_body.hpp)    // lint-world: a body without a payload
#include WEBCPP_BEAST_BACKEND_HEADER(http/file_body.hpp)  // lint-world: explicit file-backed HTTP bodies
#include WEBCPP_BEAST_BACKEND_HEADER(http/message.hpp)  // lint-world: HTTP message data only
#include WEBCPP_BEAST_BACKEND_HEADER(http/parser.hpp)  // lint-world: parses caller-supplied bytes only
#include WEBCPP_BEAST_BACKEND_HEADER(http/serializer.hpp)  // lint-world: serializes into caller-owned buffers
#include WEBCPP_BEAST_BACKEND_HEADER(http/string_body.hpp)  // lint-world: memory buffers only

namespace webcpp::beast::http {
using namespace ::webcpp::beast_backend::http;
/** Upstream body backed by a selected dynamic buffer type. */
using ::webcpp::beast_backend::http::basic_dynamic_body;
/** Upstream HTTP fields with an application-supplied allocator. */
using ::webcpp::beast_backend::http::basic_fields;
/** Upstream string body with an application-supplied allocator. */
using ::webcpp::beast_backend::http::basic_string_body;
/** Upstream body that reads or writes caller-owned buffers. */
using ::webcpp::beast_backend::http::buffer_body;
/** Upstream dynamically buffered body. */
using ::webcpp::beast_backend::http::dynamic_body;
/** Upstream body for messages without a payload. */
using ::webcpp::beast_backend::http::empty_body;
/** Upstream HTTP parser and serializer errors. */
using ::webcpp::beast_backend::http::error;
/** Upstream HTTP field names. */
using ::webcpp::beast_backend::http::field;
/** Upstream HTTP fields. */
using ::webcpp::beast_backend::http::fields;
/** Upstream file-backed message body. */
using ::webcpp::beast_backend::http::file_body;
/** Upstream request or response header. */
using ::webcpp::beast_backend::http::header;
/** Upstream HTTP request or response message. */
using ::webcpp::beast_backend::http::message;
/** Returns the conventional reason phrase for a status code. */
using ::webcpp::beast_backend::http::obsolete_reason;
/** Upstream incremental message parser. */
using ::webcpp::beast_backend::http::parser;
/** Upstream HTTP request. */
using ::webcpp::beast_backend::http::request;
/** Upstream HTTP request header. */
using ::webcpp::beast_backend::http::request_header;
/** Upstream incremental request parser. */
using ::webcpp::beast_backend::http::request_parser;
/** Upstream incremental request serializer. */
using ::webcpp::beast_backend::http::request_serializer;
/** Upstream HTTP response. */
using ::webcpp::beast_backend::http::response;
/** Upstream HTTP response header. */
using ::webcpp::beast_backend::http::response_header;
/** Upstream incremental response parser. */
using ::webcpp::beast_backend::http::response_parser;
/** Upstream incremental response serializer. */
using ::webcpp::beast_backend::http::response_serializer;
/** Upstream incremental message serializer. */
using ::webcpp::beast_backend::http::serializer;
/** Upstream HTTP status codes. */
using ::webcpp::beast_backend::http::status;
/** Upstream string message body. */
using ::webcpp::beast_backend::http::string_body;
/** Parses a method name while preserving upstream method semantics. */
using ::webcpp::beast_backend::http::string_to_verb;
/** Upstream HTTP methods. */
using ::webcpp::beast_backend::http::verb;
}  // namespace webcpp::beast::http

#endif
