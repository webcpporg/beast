// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)
// See accompanying file LICENSE_1_0.txt or https://www.boost.org/LICENSE_1_0.txt.

#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/ip/tcp.hpp>
#include <webcpp/asio/ssl/stream.hpp>
#include <webcpp/beast/websocket/ssl.hpp>
#include <webcpp/beast/websocket/stream.hpp>

int main() {
    webcpp::asio::io_context context;
    webcpp::asio::ssl::context tls(webcpp::asio::ssl::context::tls_client);
    using transport = webcpp::asio::ssl::stream<webcpp::asio::ip::tcp::socket>;
    webcpp::beast::websocket::stream<transport> socket(context, tls);
    socket.async_close(webcpp::beast::websocket::close_code::normal,
                       [](boost::system::error_code) {});
    // Compile-only: instantiates the TLS teardown customization through public headers.
}
