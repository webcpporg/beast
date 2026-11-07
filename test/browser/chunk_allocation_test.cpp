// Copyright (c) 2026 WebCpp.org
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <webcpp/asio/buffer.hpp>
#include <webcpp/beast/http.hpp>

#include <array>
#include <boost/throw_exception.hpp>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <new>
#include <string>

namespace {
// Replacement allocation functions observe only the controlled serializer call.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
bool reject_allocations = false;

using fields = webcpp::beast::http::basic_fields<std::pmr::polymorphic_allocator<char>>;

bool serialize(bool chunked) {
    namespace http = webcpp::beast::http;
    std::array<std::byte, 4096> storage{};
    std::pmr::monotonic_buffer_resource memory(storage.data(), storage.size(),
                                               std::pmr::null_memory_resource());
    http::request<http::string_body, fields> request(
        http::verb::post, "/", 11, "test body", std::pmr::polymorphic_allocator<char>(&memory));
    if (chunked) {
        request.chunked(true);
    } else {
        request.prepare_payload();
    }
    http::request_serializer<http::string_body, fields> serializer(request);
    boost::system::error_code ec;
    std::string wire;
    while (!serializer.is_done()) {
        serializer.next(ec, [&](auto&, const auto& buffers) {
            for (const auto buffer : buffers) {
                wire.append(static_cast<const char*>(buffer.data()), buffer.size());
            }
            serializer.consume(webcpp::asio::buffer_size(buffers));
        });
        if (ec) {
            return false;
        }
    }
    return wire.ends_with(chunked ? "9\r\ntest body\r\n0\r\n\r\n" : "test body");
}

#ifdef WEBCPP_HAS_EXCEPTIONS
bool allocation_failure(bool chunked) {
    namespace http = webcpp::beast::http;
    // Keep fields in caller storage so the rejected allocation belongs to chunk state.
    std::array<std::byte, 4096> storage{};
    std::pmr::monotonic_buffer_resource memory(storage.data(), storage.size(),
                                               std::pmr::null_memory_resource());
    http::request<http::string_body, fields> request(
        http::verb::post, "/", 11, "test body", std::pmr::polymorphic_allocator<char>(&memory));
    if (chunked) {
        request.chunked(true);
    } else {
        request.prepare_payload();
    }
    http::request_serializer<http::string_body, fields> serializer(request);
    boost::system::error_code ec;
    reject_allocations = true;
    try {
        serializer.next(ec, [](auto&, const auto&) {});
    } catch (const std::bad_alloc&) {
        reject_allocations = false;
        return chunked;
    }
    reject_allocations = false;
    return !chunked && !ec;
}
#endif
}  // namespace

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name): standard replacement signatures
void* operator new(std::size_t size) {
    if (reject_allocations) {
        boost::throw_exception(std::bad_alloc());
    }
    if (auto* pointer = std::malloc(size == 0 ? 1 : size)) {
        return pointer;
    }
    boost::throw_exception(std::bad_alloc());
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
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

// NOLINTEND(readability-inconsistent-declaration-parameter-name)

int main() {
    if (!serialize(false) || !serialize(true)) {
        return 1;
    }
#ifdef WEBCPP_HAS_EXCEPTIONS
    if (!allocation_failure(false) || !allocation_failure(true)) {
        return 2;
    }
    // A failed serializer can be destroyed; a fresh operation still works.
    if (!serialize(true)) {
        return 3;
    }
    std::puts("chunk allocation: bad_alloc propagated");
#endif
    std::puts("browser chunk allocation: ok");
}
