#pragma once

#include <string>
#include <string_view>
#include <format>
#include <nats/nats.h>

namespace nats
{
using std::string;
using std::string_view;

struct NatsError
{
    /**
     * NATS status code.
     */
    natsStatus status;

    /**
     * NATS status text from `natsStatus_GetText`.
     */
    string_view status_text;

    /**
     * Error message provided by the `NatsClient` c++ wrapper.
     */
    string message;

    NatsError(natsStatus s, string message) noexcept //
        : status(s), message(message)
    {
        status_text = natsStatus_GetText(s);
    }

    string to_string() const noexcept
    {
        return std::format("NATS error {}: {} - {}", (int)status, status_text, message);
    }
};
}
