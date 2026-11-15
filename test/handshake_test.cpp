// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

// A client's WebSocket handshake over the bounded next layer, against local stand-ins that refuse
// the upgrade with a large body or answer it with a long header. A counting layer below it records
// the bytes the client reads from TCP, and the global allocation functions count what the
// handshake allocates, so each case sees how much a hostile server could make it read and hold.

#include <webcpp/beast.hpp>
#include <webcpp/beast/http/read.hpp>
#include <webcpp/beast/tcp_stream.hpp>
#include <webcpp/beast/websocket.hpp>
#include <webcpp/beast/websocket/bounded_handshake_stream.hpp>

#include <webcpp/asio/async_result.hpp>
#include <webcpp/asio/buffer.hpp>
#include <webcpp/asio/io_context.hpp>
#include <webcpp/asio/ip/tcp.hpp>
#include <webcpp/asio/steady_timer.hpp>
#include <webcpp/asio/write.hpp>

#include <boost/core/lightweight_test.hpp>
#include <boost/throw_exception.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>
#include <string>
#include <type_traits>
#include <utility>

namespace beast = webcpp::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = webcpp::asio::ip::tcp;
using boost::system::error_code;
using namespace std::chrono_literals;

namespace {
// Global replacement operators can reach only process-wide test counters.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
bool measuring = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::size_t allocated = 0;

void* allocate(std::size_t size, std::size_t alignment) {
    if (measuring) {
        allocated += size;
    }
    alignment = std::max(alignment, alignof(std::max_align_t));
    auto* pointer = std::aligned_alloc(
        alignment, ((std::max(size, std::size_t{1}) + alignment - 1) / alignment) * alignment);
    if (pointer == nullptr) {
        boost::throw_exception(std::bad_alloc());
    }
    return pointer;
}
}  // namespace

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name): standard replacement signatures
void* operator new(std::size_t size) {
    return allocate(size, alignof(std::max_align_t));
}

void* operator new[](std::size_t size) {
    return allocate(size, alignof(std::max_align_t));
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t /*unused*/) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t /*unused*/) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::align_val_t /*unused*/) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::align_val_t /*unused*/) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t /*unused*/, std::align_val_t /*unused*/) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t /*unused*/,
                       std::align_val_t /*unused*/) noexcept {
    std::free(pointer);
}

// NOLINTEND(readability-inconsistent-declaration-parameter-name)

namespace {

constexpr std::size_t mebibyte = std::size_t{1024} * 1024;
// Far below the megabyte a refusal sends, far above what a bounded handshake allocates.
constexpr std::size_t allocation_ceiling = std::size_t{64} * 1024;

/** The first `size` bytes of `buffers`, as a plain buffer when the sequence is one buffer. */
template <class Buffers>
auto prefix(std::size_t size, const Buffers& buffers) {
    if constexpr (std::is_convertible_v<Buffers, webcpp::asio::mutable_buffer>) {
        // Beast cannot take a prefix of its own single-buffer prefix, as its TCP stream does.
        return webcpp::asio::buffer(webcpp::asio::mutable_buffer(buffers), size);
    } else {
        return beast::buffers_prefix(size, buffers);
    }
}

/**
 A TCP stream that counts the bytes read from it and reads at most `chunk` bytes at a time, so that
 a header's lines can be made to span reads.
*/
class counted_stream {
    beast::tcp_stream next_;
    std::size_t* read_;
    std::size_t chunk_;

public:
    using executor_type = beast::tcp_stream::executor_type;

    counted_stream(webcpp::asio::io_context& context, std::size_t& read,
                   std::size_t chunk = std::numeric_limits<std::size_t>::max())
        : next_(context), read_(&read), chunk_(chunk) {}

    executor_type get_executor() noexcept { return next_.get_executor(); }

    beast::tcp_stream& next_layer() noexcept { return next_; }

    [[nodiscard]] const beast::tcp_stream& next_layer() const noexcept { return next_; }

    template <class Buffers>
    std::size_t read_some(const Buffers& buffers, error_code& ec) {
        const auto transferred = next_.read_some(prefix(chunk_, buffers), ec);
        *read_ += transferred;
        return transferred;
    }

    template <class Buffers>
    std::size_t write_some(const Buffers& buffers, error_code& ec) {
        return next_.write_some(buffers, ec);
    }

    template <class Buffers, class Token>
    auto async_read_some(const Buffers& buffers, Token&& token) {
        return webcpp::asio::async_initiate<Token, void(error_code, std::size_t)>(
            [this](auto handler, const Buffers& target) {
                next_.async_read_some(prefix(chunk_, target),
                                      [read = read_, handler = std::move(handler)](
                                          error_code ec, std::size_t transferred) mutable {
                                          *read += transferred;
                                          std::move(handler)(ec, transferred);
                                      });
            },
            token, buffers);
    }

    template <class Buffers, class Token>
    auto async_write_some(const Buffers& buffers, Token&& token) {
        return next_.async_write_some(buffers, std::forward<Token>(token));
    }

    template <class Handler>
    friend void async_teardown(beast::role_type role, counted_stream& stream, Handler&& handler) {
        using webcpp::beast_backend::websocket::async_teardown;
        async_teardown(role, stream.next_, std::forward<Handler>(handler));
    }
};

using bounded_layer = websocket::bounded_handshake_stream<counted_stream>;
using client_stream = websocket::stream<bounded_layer>;

/** What a client's handshake did: its result, the response, and what it read and allocated. */
struct handshake_result {
    error_code error;
    std::size_t read = 0;
    std::size_t allocated = 0;
    websocket::response_type response;
};

/** Starts measuring what the handshake allocates. */
void start_measuring() {
    measuring = true;
    allocated = 0;
}

/** Stops measuring, and returns what the handshake allocated. */
std::size_t stop_measuring() {
    measuring = false;
    return allocated;
}

/**
 A handshake against a stand-in that reads the upgrade request, answers `answer` and then ends
 its output, as a server that refuses the upgrade does.
*/
class answer_exchange {
    webcpp::asio::io_context context_;
    tcp::acceptor listener_{context_, {webcpp::asio::ip::address_v4::loopback(), 0}};
    tcp::socket peer_{context_};
    const std::string& answer_;
    handshake_result& result_;
    client_stream client_;
    beast::flat_buffer input_;
    http::request<http::string_body> request_;
    webcpp::asio::steady_timer deadline_{context_, 5s};

    void expired(error_code ec) {
        if (!ec) {
            BOOST_ERROR("Refused handshake exceeded its deadline");
            beast::get_lowest_layer(client_).close();
            BOOST_TEST(!peer_.close(ec));
        }
    }

    void accepted(error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        http::async_read(peer_, input_, request_,
                         beast::bind_front_handler(&answer_exchange::received, this));
    }

    void received(error_code ec, [[maybe_unused]] std::size_t transferred) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        // The write ends when the client has read it all, or when the client closes.
        webcpp::asio::async_write(peer_, webcpp::asio::buffer(answer_),
                                  beast::bind_front_handler(&answer_exchange::answered, this));
    }

    void answered(error_code ec, [[maybe_unused]] std::size_t transferred) {
        if (!ec) {
            // The end of the output ends a body that runs to it.
            BOOST_TEST(!peer_.shutdown(tcp::socket::shutdown_send, ec));
        }
    }

    void connected(error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        start_measuring();
        client_.async_handshake(result_.response, "localhost", "/relay",
                                beast::bind_front_handler(&answer_exchange::handshaken, this));
    }

    void handshaken(error_code ec) {
        result_.allocated = stop_measuring();
        result_.error = ec;
        beast::get_lowest_layer(client_).close();
        deadline_.cancel();
    }

public:
    answer_exchange(const std::string& answer, websocket::response_limits limits, std::size_t chunk,
                    handshake_result& result)
        : answer_(answer), result_(result), client_(context_, result.read, chunk) {
        client_.next_layer().limits(limits);
    }

    void run() {
        deadline_.async_wait(beast::bind_front_handler(&answer_exchange::expired, this));
        listener_.async_accept(peer_, beast::bind_front_handler(&answer_exchange::accepted, this));
        beast::get_lowest_layer(client_).async_connect(
            listener_.local_endpoint(),
            beast::bind_front_handler(&answer_exchange::connected, this));
        context_.run();  // lint-run: drive the refused handshake against its local stand-in
    }
};

/** Runs a handshake against a stand-in that answers `answer`, then ends its output. */
handshake_result against_answer(const std::string& answer, websocket::response_limits limits,
                                std::size_t chunk = std::numeric_limits<std::size_t>::max()) {
    handshake_result result;
    answer_exchange exchange(answer, limits, chunk, result);
    exchange.run();
    return result;
}

/**
 A handshake against a Beast server whose 101 response carries a field of `filler` bytes, then,
 when the handshake succeeds, a binary echo and the closing handshake.
*/
template <class Client>
class upgrade_exchange {
    webcpp::asio::io_context& context_;
    Client& client_;
    handshake_result& result_;
    tcp::acceptor listener_{context_, {webcpp::asio::ip::address_v4::loopback(), 0}};
    websocket::stream<beast::tcp_stream> server_{context_};
    beast::flat_buffer server_data_;
    beast::flat_buffer client_data_;
    const std::string payload{"binary\0message", 14};
    bool echoed_ = false;
    webcpp::asio::steady_timer deadline_{context_, 5s};

    void expired(error_code ec) {
        if (!ec) {
            BOOST_ERROR("Upgrade exceeded its deadline");
            beast::get_lowest_layer(client_).close();
            beast::get_lowest_layer(server_).close();
        }
    }

    void accepted(error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        server_.async_accept(beast::bind_front_handler(&upgrade_exchange::upgraded, this));
    }

    void upgraded(error_code ec) {
        if (ec) {
            return;  // the client gave up on a response it bounded
        }
        server_.async_read(server_data_, beast::bind_front_handler(&upgrade_exchange::echo, this));
    }

    void echo(error_code ec, [[maybe_unused]] std::size_t transferred) {
        if (ec) {
            return;  // the client's close, or the connection it dropped
        }
        server_.binary(true);
        server_.async_write(server_data_.data(),
                            beast::bind_front_handler(&upgrade_exchange::echoed, this));
    }

    void echoed(error_code ec, [[maybe_unused]] std::size_t transferred) {
        BOOST_TEST(!ec);
        server_data_.consume(server_data_.size());
        server_.async_read(server_data_, [](error_code, std::size_t) {});
    }

    void connected(error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        start_measuring();
        client_.async_handshake(result_.response, "localhost", "/relay",
                                beast::bind_front_handler(&upgrade_exchange::handshaken, this));
    }

    void handshaken(error_code ec) {
        result_.allocated = stop_measuring();
        result_.error = ec;
        if (ec) {
            beast::get_lowest_layer(client_).close();
            deadline_.cancel();
            return;
        }
        client_.binary(true);
        client_.async_write(webcpp::asio::buffer(payload),
                            beast::bind_front_handler(&upgrade_exchange::written, this));
    }

    void written(error_code ec, [[maybe_unused]] std::size_t transferred) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        client_.async_read(client_data_, beast::bind_front_handler(&upgrade_exchange::read, this));
    }

    void read(error_code ec, [[maybe_unused]] std::size_t transferred) {
        BOOST_TEST(!ec);
        echoed_ = beast::buffers_to_string(client_data_.data()) == payload;
        client_.async_close(websocket::close_code::normal,
                            beast::bind_front_handler(&upgrade_exchange::closed, this));
    }

    void closed(error_code ec) {
        BOOST_TEST(!ec);
        deadline_.cancel();
    }

public:
    upgrade_exchange(webcpp::asio::io_context& context, Client& client, std::size_t filler,
                     handshake_result& result)
        : context_(context), client_(client), result_(result) {
        server_.set_option(
            websocket::stream_base::decorator([filler](websocket::response_type& response) {
                if (filler != 0) {
                    response.set("X-Filler", std::string(filler, 'f'));
                }
            }));
    }

    void run() {
        deadline_.async_wait(beast::bind_front_handler(&upgrade_exchange::expired, this));
        listener_.async_accept(beast::get_lowest_layer(server_).socket(),
                               beast::bind_front_handler(&upgrade_exchange::accepted, this));
        beast::get_lowest_layer(client_).async_connect(
            listener_.local_endpoint(),
            beast::bind_front_handler(&upgrade_exchange::connected, this));
        context_.run();  // lint-run: drive the upgrade, the echo and the closing handshake
        if (!result_.error) {
            BOOST_TEST(echoed_);
        }
    }
};

/** Runs a handshake against a Beast server whose 101 carries a field of `filler` bytes. */
template <class Client>
handshake_result against_server(Client& client, webcpp::asio::io_context& context,
                                std::size_t filler) {
    handshake_result result;
    upgrade_exchange<Client> exchange(context, client, filler, result);
    exchange.run();
    return result;
}

// A refusal that declares a megabyte fails at its header, before the body is read or reserved.
void test_declared_body() {
    const auto answer = "HTTP/1.1 403 Forbidden\r\nContent-Length: " + std::to_string(mebibyte) +
                        "\r\n\r\n" + std::string(mebibyte, 'x');
    const websocket::response_limits limits;
    const auto result = against_answer(answer, limits);
    BOOST_TEST(result.error == http::error::body_limit);
    BOOST_TEST_LE(result.read, std::size_t{limits.header});
    BOOST_TEST_LT(result.allocated, allocation_ceiling);
}

// A refusal whose body runs to the end of the stream stops after the bound.
void test_streamed_body() {
    const auto answer =
        "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\n" + std::string(mebibyte, 'x');
    const websocket::response_limits limits;
    const auto result = against_answer(answer, limits);
    BOOST_TEST(result.error == http::error::body_limit);
    BOOST_TEST_LE(result.read, std::size_t{limits.header} + limits.body);
    BOOST_TEST_LT(result.allocated, allocation_ceiling);
}

// A chunked refusal is bounded by its bytes on the wire, its chunk framing included.
void test_chunked_body() {
    std::string answer = "HTTP/1.1 403 Forbidden\r\nTransfer-Encoding: chunked\r\n\r\n";
    for (int i = 0; i < 1024; ++i) {
        answer += "400\r\n" + std::string(1024, 'x') + "\r\n";
    }
    answer += "0\r\n\r\n";
    const websocket::response_limits limits;
    const auto result = against_answer(answer, limits);
    BOOST_TEST(result.error == http::error::body_limit);
    BOOST_TEST_LE(result.read, std::size_t{limits.header} + limits.body);
    BOOST_TEST_LT(result.allocated, allocation_ceiling);
}

// A refusal within the bound is read whole, as upstream reads it, a few bytes at a time here.
void test_short_refusal() {
    const std::string answer =
        "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nContent-Length: 9\r\n\r\nForbidden";
    const websocket::response_limits limits;
    const auto result = against_answer(answer, limits, 7);
    BOOST_TEST(result.error == websocket::error::upgrade_declined);
    BOOST_TEST(result.response.result() == http::status::forbidden);
    BOOST_TEST_EQ(result.response.body(), "Forbidden");
    BOOST_TEST_EQ(result.read, answer.size());
}

// A body bound of zero still reads a refusal without a body, and fails one with a body unread.
void test_discarded_body() {
    const std::string answer = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\n\r\n";
    const auto result = against_answer(answer, {.body = 0});
    BOOST_TEST(result.error == websocket::error::upgrade_declined);
    BOOST_TEST(result.response.result() == http::status::too_many_requests);

    const auto refused =
        against_answer("HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden", {.body = 0});
    BOOST_TEST(refused.error == http::error::body_limit);
}

// A 101 whose header is longer than the default bound fails after reading at most that bound.
// The server shares the process and builds that header, so only the client's reads are measured.
void test_long_header() {
    webcpp::asio::io_context context;
    std::size_t read = 0;
    client_stream client(context, read);
    const auto result = against_server(client, context, std::size_t{32} * 1024);
    BOOST_TEST(result.error == http::error::header_limit);
    BOOST_TEST_LE(read, std::size_t{websocket::response_limits{}.header});
}

// A header bound below a valid 101's header fails the upgrade that the default bound accepts.
void test_header_bound() {
    {
        webcpp::asio::io_context context;
        std::size_t read = 0;
        client_stream client(context, read);
        client.next_layer().limits({.header = 1024});
        const auto result = against_server(client, context, 4096);
        BOOST_TEST(result.error == http::error::header_limit);
        BOOST_TEST_LE(read, std::size_t{1024});
    }
    {
        webcpp::asio::io_context context;
        std::size_t read = 0;
        client_stream client(context, read);
        const auto result = against_server(client, context, 4096);
        BOOST_TEST(!result.error);
        BOOST_TEST(result.response.result() == http::status::switching_protocols);
    }
}

/** Counts what a resource allocates and still holds. */
class counting_resource : public std::pmr::memory_resource {
public:
    std::size_t allocations = 0;
    std::size_t outstanding = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        ++allocations;
        outstanding += bytes;
        return std::pmr::new_delete_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
        outstanding -= bytes;
        std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
    }

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

// A successful upgrade passes through, its header lines spanning reads of a few bytes, which the
// layer keeps in the allocator it is given, and releases once the header is read.
void test_upgrade() {
    using pmr_layer =
        websocket::bounded_handshake_stream<counted_stream, std::pmr::polymorphic_allocator<char>>;
    counting_resource resource;
    webcpp::asio::io_context context;
    std::size_t read = 0;
    websocket::stream<pmr_layer> client(std::allocator_arg,
                                        std::pmr::polymorphic_allocator<char>(&resource), context,
                                        read, std::size_t{7});
    const auto result = against_server(client, context, 0);
    BOOST_TEST(!result.error);
    BOOST_TEST(result.response.result() == http::status::switching_protocols);
    BOOST_TEST_GT(resource.allocations, std::size_t{0});
    BOOST_TEST_EQ(resource.outstanding, std::size_t{0});
}

// The synchronous handshake reads through the same bound.
void test_synchronous() {
    webcpp::asio::io_context context;
    tcp::acceptor listener(context, {webcpp::asio::ip::address_v4::loopback(), 0});
    std::size_t read = 0;
    client_stream client(context, read);
    error_code ec;
    if (!BOOST_TEST(
            !beast::get_lowest_layer(client).socket().connect(listener.local_endpoint(), ec))) {
        return;
    }
    tcp::socket peer(context);
    if (!BOOST_TEST(!listener.accept(peer, ec))) {
        return;
    }
    // Answered before the request is read, so that one thread can play both parts.
    const std::string answer =
        "HTTP/1.1 403 Forbidden\r\nContent-Length: " + std::to_string(mebibyte) + "\r\n\r\nxxxx";
    webcpp::asio::write(peer, webcpp::asio::buffer(answer), ec);
    if (!BOOST_TEST(!ec)) {
        return;
    }
    // A client that read past the header would otherwise wait for the rest of the body forever.
    BOOST_TEST(!peer.shutdown(tcp::socket::shutdown_send, ec));
    websocket::response_type response;
    client.handshake(response, "localhost", "/relay", ec);
    BOOST_TEST(ec == http::error::body_limit);
    BOOST_TEST_LE(read, std::size_t{websocket::response_limits{}.header});
}

// The page's example: a client that reads no byte of a refusal's body.
void test_documented() {
    webcpp::asio::io_context context;
    tcp::acceptor listener(context, {webcpp::asio::ip::address_v4::loopback(), 0});
    tcp::socket peer(context);
    const std::string answer = "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
    beast::flat_buffer input;
    http::request<http::string_body> request;
    bool refused = false;
    listener.async_accept(peer, [&](error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        http::async_read(peer, input, request, [&](error_code read_error, std::size_t) {
            if (!read_error) {
                webcpp::asio::async_write(peer, webcpp::asio::buffer(answer),
                                          [](error_code, std::size_t) {});
            }
        });
    });
    // tag::bounded-handshake[]
    // Beast's WebSocket stream over the bounded layer, over its transport: TCP here.
    websocket::stream<websocket::bounded_handshake_stream<beast::tcp_stream>> ws(context);
    // At most 4 KiB of response header, and no byte of a refusal's body.
    ws.next_layer().limits({.header = 4 * 1024, .body = 0});
    // end::bounded-handshake[]
    webcpp::asio::steady_timer deadline(context, 5s);
    deadline.async_wait([&](error_code ec) {
        if (!ec) {
            BOOST_ERROR("Documented handshake exceeded its deadline");
            beast::get_lowest_layer(ws).close();
            BOOST_TEST(!peer.close(ec));
        }
    });
    beast::get_lowest_layer(ws).async_connect(listener.local_endpoint(), [&](error_code ec) {
        if (!BOOST_TEST(!ec)) {
            return;
        }
        // tag::bounded-handshake-call[]
        ws.async_handshake("localhost", "/relay", [&](error_code handshake_error) {
            // The server refused with a body: the handshake fails before reading it.
            refused = handshake_error == http::error::body_limit;
            beast::get_lowest_layer(ws).close();
            deadline.cancel();
        });
        // end::bounded-handshake-call[]
    });
    context.run();  // lint-run: drive the documented handshake against its local stand-in
    BOOST_TEST(refused);
}

void test_defaults() {
    constexpr websocket::response_limits limits;
    static_assert(limits.header == 8 * 1024);
    static_assert(limits.body == 1024);
    static_assert(beast::is_async_stream<bounded_layer>::value);
    static_assert(beast::is_sync_stream<bounded_layer>::value);
    static_assert(std::is_same_v<beast::lowest_layer_type<bounded_layer>, beast::tcp_stream>);
}

}  // namespace

int main() {
    test_defaults();
    test_declared_body();
    test_streamed_body();
    test_chunked_body();
    test_short_refusal();
    test_discarded_body();
    test_long_header();
    test_header_bound();
    test_upgrade();
    test_synchronous();
    test_documented();
    return boost::report_errors();
}
