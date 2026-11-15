// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#ifndef WEBCPP_BEAST_WEBSOCKET_BOUNDED_HANDSHAKE_STREAM_HPP
#define WEBCPP_BEAST_WEBSOCKET_BOUNDED_HANDSHAKE_STREAM_HPP

#include <webcpp/beast/config.hpp>

#include WEBCPP_ASIO_BACKEND_HEADER(async_result.hpp)      // lint-world: completion tokens only
#include WEBCPP_ASIO_BACKEND_HEADER(buffer.hpp)            // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/async_base.hpp)  // lint-world: completion handlers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/buffers_prefix.hpp)  // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/flat_buffer.hpp)     // lint-world: memory buffers only
#include WEBCPP_BEAST_BACKEND_HEADER(core/role.hpp)            // lint-world: teardown role only
#include WEBCPP_BEAST_BACKEND_HEADER(http/basic_parser.hpp)    // lint-world: parses bytes only
#include WEBCPP_BEAST_BACKEND_HEADER(http/error.hpp)           // lint-world: error codes only
#include WEBCPP_BEAST_BACKEND_HEADER(websocket/teardown.hpp)   // lint-world: forwards teardown
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace webcpp::beast::detail {
/** Reads a handshake response's status line and fields with Beast's parser, keeping its status. */
class handshake_response_parser final : public ::webcpp::beast_backend::http::basic_parser<false> {
    using error_code = boost::system::error_code;
    using string_view = ::webcpp::beast_backend::string_view;

    int status_ = 0;

protected:
    void on_request_impl(::webcpp::beast_backend::http::verb /*method*/,
                         string_view /*method_string*/, string_view /*target*/, int /*version*/,
                         error_code& /*ec*/) override {}

    void on_response_impl(int code, string_view /*reason*/, int /*version*/,
                          error_code& /*ec*/) override {
        status_ = code;
    }

    void on_field_impl(::webcpp::beast_backend::http::field /*name*/, string_view /*name_string*/,
                       string_view /*value*/, error_code& /*ec*/) override {}

    void on_trailer_field_impl(::webcpp::beast_backend::http::field /*name*/,
                               string_view /*name_string*/, string_view /*value*/,
                               error_code& /*ec*/) override {}

    void on_header_impl(error_code& /*ec*/) override {}

    void on_body_init_impl(const boost::optional<std::uint64_t>& /*content_length*/,
                           error_code& /*ec*/) override {}

    std::size_t on_body_impl(string_view body, error_code& /*ec*/) override { return body.size(); }

    void on_chunk_header_impl(std::uint64_t /*size*/, string_view /*extensions*/,
                              error_code& /*ec*/) override {}

    std::size_t on_chunk_body_impl(std::uint64_t /*remain*/, string_view body,
                                   error_code& /*ec*/) override {
        return body.size();
    }

    void on_finish_impl(error_code& /*ec*/) override {}

public:
    /** The status code of the status line, or 0 before it is read.
     @return The status code. */
    [[nodiscard]] int status() const noexcept { return status_; }
};

/** The first `size` bytes of `buffers`, as a plain buffer when the sequence is one buffer.
 @tparam MutableBufferSequence The buffer sequence type.
 @param size The most bytes to keep.
 @param buffers The buffers to take them from.
 @return A buffer sequence of at most `size` bytes. */
template <class MutableBufferSequence>
auto read_prefix(std::size_t size, const MutableBufferSequence& buffers) {
    if constexpr (std::is_convertible_v<MutableBufferSequence,
                                        ::webcpp::asio_backend::mutable_buffer>) {
        // Beast's TCP stream takes a prefix of what it is given, which it cannot do of its own
        // single-buffer prefix: a plain buffer keeps that layer working.
        return ::webcpp::asio_backend::buffer(::webcpp::asio_backend::mutable_buffer(buffers),
                                              size);
    } else {
        return ::webcpp::beast_backend::buffers_prefix(size, buffers);
    }
}
}  // namespace webcpp::beast::detail

// NOLINTNEXTLINE(modernize-concat-nested-namespaces): Emscripten adds the policy's inline namespace
namespace webcpp::beast {
#ifdef WEBCPP_TARGET_EMSCRIPTEN
// The page's facade declares its WebSocket names in the unsupported-setting policy's namespace.
#ifdef WEBCPP_BEAST_UNSUPPORTED_IGNORE
inline namespace unsupported_ignore {
#else
inline namespace unsupported_error {
#endif
#endif
namespace websocket {

/**
 Bounds on the HTTP response that a client's WebSocket handshake reads.

 A response that accepts the upgrade, status 101, ends with its header. A response that refuses it
 may carry a body, which the handshake reads into its response, as upstream does, up to `body`
 bytes.
*/
struct response_limits {
    /**
     The most bytes of the status line and fields, line ends included, defaulting to 8 KiB.

     It is Beast's own limit on a response header, which the handshake's parser also keeps, so a
     larger value reads no more.
    */
    std::uint32_t header = 8 * 1024;
    /**
     The most bytes read after the header of a response that refuses the upgrade, defaulting to
     1 KiB.

     They are counted as they arrive, chunk framing included; a declared `Content-Length` above it
     fails as soon as the header is read. Zero discards every refusal's body.
    */
    std::uint64_t body = 1024;
};

/**
 A client's next layer that bounds the HTTP response its WebSocket handshake reads.

 Upstream's handshake reads the server's response with a parser of Beast's default limits, 8 KiB of
 header and 8 MiB of body, whatever the status, in memory no allocator of the caller's reaches. A
 hostile server can thus answer each connection with a refusal of 8 MiB. Placed between the
 WebSocket stream and its transport, this layer reads the response's header through Beast's own
 parser with `response_limits::header` and `response_limits::body` as its limits, and hands the
 handshake no more than these bounds allow.

 A header longer than the bound fails the read with `http::error::header_limit`, after at most
 `response_limits::header` bytes. A refusal that declares a longer body fails it with
 `http::error::body_limit` once its header is read, before the handshake reserves anything for it,
 and one that sends more than `response_limits::body` bytes after its header fails it with the
 same error, once they have arrived or when the handshake asks for them. A response of status 101
 ends the layer's work: every later read and write is the next layer's, unchanged.

 In all, the layer reads at most `header + body` bytes of a response before its error. The
 handshake then fails with that error, as upstream's fails when its own parser's limits are
 exceeded, its response left as upstream leaves it then. Beast's types and algorithms are
 unchanged: natively the WebSocket stream above is Boost.Beast's own, on WASI the shipped copy of
 it. On Emscripten the page's WebSocket reads its own response; there the layer serves only the
 guest engine of `<webcpp/beast/websocket/guest.hpp>` over a supplied channel.

 @code
 using transport = beast::ssl_stream<beast::tcp_stream>;
 websocket::stream<websocket::bounded_handshake_stream<transport>> ws(executor, tls);
 ws.next_layer().limits({.body = 0});  // a refusal's body is never read
 // The TLS handshake is one layer further down, then the WebSocket handshake as before.
 ws.next_layer().next_layer().async_handshake(ssl::stream_base::client, on_tls);
 ws.async_handshake(host, target, on_upgrade);
 @endcode

 @tparam NextLayer The transport, an AsyncStream or SyncStream such as `tcp_stream` or
 `ssl_stream<tcp_stream>`, which the layer constructs from its arguments.
 @tparam Allocator The allocator that keeps the unparsed part of a header line split between two
 reads; a line read whole needs no memory.
 @note For a client's handshake only: a server's accept reads a request, which fails this layer's
 response parser. A WebSocket stream constructed with `std::allocator_arg` and an allocator passes
 them on to this layer, natively as upstream does and on WASI because the layer declares
 `allocator_type`.
 @see response_limits
*/
template <class NextLayer, class Allocator = std::allocator<char>>
class bounded_handshake_stream {
    enum class phase : std::uint8_t { header, body, open, failed };

    NextLayer next_;
    response_limits limits_;
    ::webcpp::beast::detail::handshake_response_parser parser_;
    ::webcpp::beast_backend::basic_flat_buffer<Allocator> pending_;
    std::uint64_t allowance_ = 0;
    phase phase_ = phase::header;
    boost::system::error_code error_;

    // The bytes the next read may take; zero, with the error that ends the handshake, past them.
    std::size_t allowance(boost::system::error_code& ec) {
        if (phase_ == phase::failed) {
            ec = error_;
            return 0;
        }
        if (allowance_ == 0) {
            ec = fail(phase_ == phase::header ? ::webcpp::beast_backend::http::error::header_limit
                                              : ::webcpp::beast_backend::http::error::body_limit);
            return 0;
        }
        return static_cast<std::size_t>(
            std::min<std::uint64_t>(allowance_, std::numeric_limits<std::size_t>::max()));
    }

    boost::system::error_code fail(boost::system::error_code ec) {
        error_ = ec;
        phase_ = phase::failed;
        pending_.clear();
        pending_.shrink_to_fit();
        return ec;
    }

    // Accounts for `transferred` bytes just read into `buffers`; an error withholds them.
    template <class MutableBufferSequence>
    boost::system::error_code on_read(const MutableBufferSequence& buffers,
                                      std::size_t transferred) {
        allowance_ -= transferred;
        if (phase_ != phase::header || transferred == 0) {
            return {};
        }
        namespace net = ::webcpp::asio_backend;
        boost::system::error_code ec;
        std::size_t after_header = 0;
        const net::mutable_buffer first = *net::buffer_sequence_begin(buffers);
        if (pending_.size() == 0 && first.size() >= transferred) {
            // Parsed where it was read; only a line that the read split is kept.
            const net::const_buffer read(first.data(), transferred);
            const auto used = parser_.put(read, ec);
            after_header = transferred - used;
            if (ec == ::webcpp::beast_backend::http::error::need_more) {
                pending_.commit(net::buffer_copy(pending_.prepare(after_header), read + used));
            }
        } else {
            pending_.commit(net::buffer_copy(pending_.prepare(transferred), buffers, transferred));
            pending_.consume(parser_.put(pending_.data(), ec));
            after_header = pending_.size();
        }
        if (ec == ::webcpp::beast_backend::http::error::need_more) {
            return {};
        }
        if (ec) {
            return fail(ec);
        }
        pending_.clear();
        pending_.shrink_to_fit();
        if (parser_.status() == 101) {
            phase_ = phase::open;
            return {};
        }
        if (after_header > limits_.body) {
            return fail(::webcpp::beast_backend::http::error::body_limit);
        }
        phase_ = phase::body;
        allowance_ = limits_.body - after_header;
        return {};
    }

    // Beast's composed operations start the layer's next operation from the completion of the
    // last: a loop of asynchronous operations, each started once the call that started the last has
    // returned. The layer starts its transport's operations through a pointer, so that the analysis
    // of calls (misc-no-recursion) sees no cycle through it, here or in a program that uses it,
    // where its reports would land in Beast's own headers.
    template <class MutableBufferSequence, class ReadToken>
    static auto transport_read(NextLayer& next, const MutableBufferSequence& buffers,
                               ReadToken&& token) {
        return next.async_read_some(buffers, std::forward<ReadToken>(token));
    }

    template <class ConstBufferSequence, class WriteToken>
    static auto transport_write(NextLayer& next, const ConstBufferSequence& buffers,
                                WriteToken&& token) {
        return next.async_write_some(buffers, std::forward<WriteToken>(token));
    }

    template <class TeardownHandler>
    static void transport_teardown(::webcpp::beast_backend::role_type role, NextLayer& next,
                                   TeardownHandler&& handler) {
        using ::webcpp::beast_backend::websocket::async_teardown;
        async_teardown(role, next, std::forward<TeardownHandler>(handler));
    }

    // A read of the response, within its bounds.
    template <class MutableBufferSequence, class ReadHandler>
    class read_op : public ::webcpp::beast_backend::async_base<ReadHandler,
                                                               typename NextLayer::executor_type> {
        bounded_handshake_stream& stream_;
        MutableBufferSequence buffers_;

    public:
        template <class Handler>
        read_op(Handler&& handler, bounded_handshake_stream& stream,
                const MutableBufferSequence& buffers)
            : ::webcpp::beast_backend::async_base<ReadHandler, typename NextLayer::executor_type>(
                  std::forward<Handler>(handler), stream.get_executor()),
              stream_(stream),
              buffers_(buffers) {}

        // Starts the transport's read, or completes at once, as if posted, past the bounds.
        static void start(read_op op) {
            boost::system::error_code ec;
            const auto allowed = op.stream_.allowance(ec);
            if (ec) {
                op.complete(false, ec, std::size_t{0});
                return;
            }
            const auto prefix = ::webcpp::beast::detail::read_prefix(allowed, op.buffers_);
            auto* const read = &transport_read<decltype(prefix), read_op>;
            read(op.stream_.next_, prefix, std::move(op));
        }

        void operator()(boost::system::error_code ec, std::size_t transferred) {
            if (const auto limit = stream_.on_read(buffers_, transferred)) {
                ec = limit;
                transferred = 0;
            }
            this->complete_now(ec, transferred);
        }
    };

    struct run_read_op {
        bounded_handshake_stream* self;

        template <class ReadHandler, class MutableBufferSequence>
        void operator()(ReadHandler&& handler, const MutableBufferSequence& buffers) const {
            if (self->phase_ == phase::open) {
                auto* const read = &transport_read<MutableBufferSequence, ReadHandler>;
                read(self->next_, buffers, std::forward<ReadHandler>(handler));
                return;
            }
            using op = read_op<MutableBufferSequence, std::decay_t<ReadHandler>>;
            auto* const start = &op::start;
            start(op(std::forward<ReadHandler>(handler), *self, buffers));
        }
    };

public:
    /** The transport below this layer. */
    using next_layer_type = NextLayer;
    /** The allocator of a split header line, which makes the layer allocator-aware. */
    using allocator_type = Allocator;
    /** The transport's executor. */
    using executor_type = NextLayer::executor_type;

    /**
     Constructs the transport from the arguments, with the default limits.
     @tparam First The first argument's type.
     @tparam Rest The other arguments' types.
     @param first The first argument of the transport's constructor.
     @param rest The other arguments of the transport's constructor.
    */
    template <class First, class... Rest>
        requires(!std::is_same_v<std::remove_cvref_t<First>, std::allocator_arg_t> &&
                 !std::is_same_v<std::remove_cvref_t<First>, bounded_handshake_stream>)
    explicit bounded_handshake_stream(First&& first, Rest&&... rest)
        : next_(std::forward<First>(first), std::forward<Rest>(rest)...) {
        limits(limits_);
    }

    /**
     Constructs the transport from the arguments, keeping a split header line in `allocator`.
     @tparam Args The transport's constructor argument types.
     @param tag Selects this constructor.
     @param allocator The allocator of a header line split between two reads.
     @param args The arguments of the transport's constructor.
    */
    template <class... Args>
    explicit bounded_handshake_stream([[maybe_unused]] std::allocator_arg_t tag,
                                      const Allocator& allocator, Args&&... args)
        : next_(std::forward<Args>(args)...), pending_(allocator) {
        limits(limits_);
    }

    /** The transport's executor.
     @return The executor. */
    executor_type get_executor() noexcept { return next_.get_executor(); }

    /** The transport below this layer.
     @return A reference to it. */
    next_layer_type& next_layer() noexcept { return next_; }

    /** The transport below this layer.
     @return A reference to it. */
    [[nodiscard]] const next_layer_type& next_layer() const noexcept { return next_; }

    /** The bounds the handshake reads within.
     @return The bounds. */
    [[nodiscard]] response_limits limits() const noexcept { return limits_; }

    /**
     Sets the bounds the handshake reads within.
     @param value The bounds.
     @pre No byte of the response has been read: set them before the handshake.
    */
    void limits(response_limits value) {
        limits_ = value;
        allowance_ = value.header;
        parser_.header_limit(value.header);
        parser_.body_limit(value.body);
    }

    /**
     Reads some bytes, within the bounds until a 101 response's header is read.
     @tparam MutableBufferSequence The buffer sequence type.
     @param buffers The buffers to read into.
     @param ec Set to the transport's error, or to the limit the response exceeded.
     @return The number of bytes read, 0 on a limit error.
    */
    template <class MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers, boost::system::error_code& ec) {
        if (phase_ == phase::open) {
            return next_.read_some(buffers, ec);
        }
        const auto allowed = allowance(ec);
        if (ec) {
            return 0;
        }
        const auto transferred =
            next_.read_some(::webcpp::beast::detail::read_prefix(allowed, buffers), ec);
        if (const auto limit = on_read(buffers, transferred)) {
            ec = limit;
            return 0;
        }
        return transferred;
    }

    /**
     Reads some bytes, within the bounds until a 101 response's header is read.
     @tparam MutableBufferSequence The buffer sequence type.
     @param buffers The buffers to read into.
     @return The number of bytes read.
     @throws boost::system::system_error On the transport's error or an exceeded limit, raised
     through `boost::throw_exception`, as Beast's SyncReadStream requires of this overload.
    */
    template <class MutableBufferSequence>
    std::size_t read_some(const MutableBufferSequence& buffers) {
        boost::system::error_code ec;
        const auto transferred = read_some(buffers, ec);
        if (ec) {
            boost::throw_exception(boost::system::system_error(ec));
        }
        return transferred;
    }

    /**
     Writes some bytes to the transport, unchanged.
     @tparam ConstBufferSequence The buffer sequence type.
     @param buffers The bytes to write.
     @param ec Set to the transport's error.
     @return The number of bytes written.
    */
    template <class ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers, boost::system::error_code& ec) {
        return next_.write_some(buffers, ec);
    }

    /**
     Writes some bytes to the transport, unchanged.
     @tparam ConstBufferSequence The buffer sequence type.
     @param buffers The bytes to write.
     @return The number of bytes written.
     @throws boost::system::system_error On the transport's error, as the transport raises it.
    */
    template <class ConstBufferSequence>
    std::size_t write_some(const ConstBufferSequence& buffers) {
        return next_.write_some(buffers);
    }

    /**
     Starts reading some bytes, within the bounds until a 101 response's header is read.

     Once that header is read, it is the transport's own read, its handler passed through.
     @tparam MutableBufferSequence The buffer sequence type.
     @tparam ReadToken The completion token for `void(error_code, std::size_t)`.
     @param buffers The buffers to read into, which must outlive the operation.
     @param token The completion token; on an exceeded limit, its error with 0 bytes.
     @return What the completion token makes of the operation.
    */
    template <class MutableBufferSequence,
              class ReadToken = ::webcpp::asio_backend::default_completion_token_t<executor_type>>
    auto async_read_some(const MutableBufferSequence& buffers, ReadToken&& token = ReadToken()) {
        return ::webcpp::asio_backend::async_initiate<ReadToken,
                                                      void(boost::system::error_code, std::size_t)>(
            run_read_op{this}, token, buffers);
    }

    /**
     Starts writing some bytes to the transport, unchanged.
     @tparam ConstBufferSequence The buffer sequence type.
     @tparam WriteToken The completion token for `void(error_code, std::size_t)`.
     @param buffers The bytes to write, which must outlive the operation.
     @param token The completion token.
     @return What the completion token makes of the operation.
    */
    template <class ConstBufferSequence,
              class WriteToken = ::webcpp::asio_backend::default_completion_token_t<executor_type>>
    auto async_write_some(const ConstBufferSequence& buffers, WriteToken&& token = WriteToken()) {
        auto* const write = &transport_write<ConstBufferSequence, WriteToken>;
        return write(next_, buffers, std::forward<WriteToken>(token));
    }

    /**
     Tears the transport down after a WebSocket close, as its own teardown does.
     @param role The local endpoint's role.
     @param stream The layer whose transport is torn down.
     @param ec Set to the transport's error.
    */
    friend void teardown(::webcpp::beast_backend::role_type role, bounded_handshake_stream& stream,
                         boost::system::error_code& ec) {
        using ::webcpp::beast_backend::websocket::teardown;
        teardown(role, stream.next_, ec);
    }

    /**
     Starts tearing the transport down after a WebSocket close, as its own teardown does.
     @tparam TeardownHandler The handler for `void(error_code)`.
     @param role The local endpoint's role.
     @param stream The layer whose transport is torn down.
     @param handler Invoked when the transport's teardown completes.
    */
    template <class TeardownHandler>
    friend void async_teardown(::webcpp::beast_backend::role_type role,
                               bounded_handshake_stream& stream, TeardownHandler&& handler) {
        auto* const teardown = &transport_teardown<TeardownHandler>;
        teardown(role, stream.next_, std::forward<TeardownHandler>(handler));
    }
};

}  // namespace websocket
#ifdef WEBCPP_TARGET_EMSCRIPTEN
}
#endif
}  // namespace webcpp::beast

#endif
