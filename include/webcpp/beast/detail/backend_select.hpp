// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.
#ifndef WEBCPP_BEAST_DETAIL_BACKEND_SELECT_HPP
#define WEBCPP_BEAST_DETAIL_BACKEND_SELECT_HPP
#include <webcpp/asio/config.hpp>
#ifdef WEBCPP_TARGET_NATIVE
// Keep header-name tokens intact; parentheses are invalid in them.
// clang-format off
/**
 Selects a protocol implementation header for internal use.

 @param header The path relative to the backend include root.
*/
#define WEBCPP_BEAST_BACKEND_HEADER(header) <boost/beast/header>  // NOLINT(bugprone-macro-parentheses) lint-world: native networking
// clang-format on
#include <boost/beast/core/detail/config.hpp>  // lint-world: native forwarding retains Boost networking

namespace webcpp {
namespace beast_backend = ::boost::beast;
}
#else
// Keep header-name tokens intact; parentheses are invalid in them.
// clang-format off
/**
 Selects a protocol implementation header for internal use.

 @param header The path relative to the backend include root.
*/
#define WEBCPP_BEAST_BACKEND_HEADER(header) <webcpp/beast/detail/backend/header>  // NOLINT(bugprone-macro-parentheses)
// clang-format on
#include <webcpp/beast/detail/backend/core/detail/config.hpp>
#endif
namespace webcpp::beast {
using namespace ::webcpp::beast_backend;
}
#endif
