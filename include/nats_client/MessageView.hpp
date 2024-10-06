#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <span>
#include <nats/nats.h>

namespace nats
{
using std::string;
using std::string_view;
using std::span;
using std::byte;

struct NatsMessageView
{
    natsMsg* ptr{nullptr};

    NatsMessageView(natsMsg* msg) noexcept //
        : ptr(msg)
    {
    }

    ~NatsMessageView() noexcept
    {
        if (ptr)
        {
            natsMsg_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Disable copy
    NatsMessageView(const NatsMessageView&) = delete;
    NatsMessageView& operator=(const NatsMessageView&) = delete;

    // Enable move
    NatsMessageView(NatsMessageView&& other) noexcept : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }
    NatsMessageView& operator=(NatsMessageView&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                natsMsg_Destroy(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    string_view subject() const noexcept
    {
        return string_view(natsMsg_GetSubject(ptr));
    }

    span<const byte> data() const noexcept
    {
        return span<const byte>(
            reinterpret_cast<const byte*>(natsMsg_GetData(ptr)), natsMsg_GetDataLength(ptr)
        );
    }

    string_view string() const noexcept
    {
        return string_view(natsMsg_GetData(ptr), natsMsg_GetDataLength(ptr));
    }

    size_t data_length() const noexcept
    {
        return natsMsg_GetDataLength(ptr);
    }
};

} // namespace nats
