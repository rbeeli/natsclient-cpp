#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <expected>
#include <nats/nats.h>

#include "Error.hpp"
#include "MessageView.hpp"

// Regex: ^natsSubscription_[a-zA-Z0-9_]+
// --------------------------------------
// OK natsSubscription_NoDeliveryDelay
// OK natsSubscription_NextMsg
// OK natsSubscription_Unsubscribe (used in NatsClient)
// OK natsSubscription_AutoUnsubscribe
// OK natsSubscription_QueuedMsgs
// OK natsSubscription_GetID
// OK natsSubscription_GetSubject
// OK natsSubscription_SetPendingLimits
// OK natsSubscription_GetPendingLimits
// OK natsSubscription_GetPending
// OK natsSubscription_GetDelivered
// OK natsSubscription_GetDropped
// OK natsSubscription_GetMaxPending
// natsSubscription_ClearMaxPending
// natsSubscription_GetStats
// OK natsSubscription_IsValid
// OK natsSubscription_Drain
// OK natsSubscription_DrainTimeout
// OK natsSubscription_WaitForDrainCompletion
// OK natsSubscription_DrainCompletionStatus
// natsSubscription_SetOnCompleteCB
// natsSubscription_Fetch
// natsSubscription_FetchRequest
// natsSubscription_GetConsumerInfo
// natsSubscription_GetSequenceMismatch
// OK natsSubscription_Destroy


namespace nats
{
using std::string;
using std::string_view;
using std::expected;

struct NatsSubscriptionSync
{
    natsSubscription* ptr;
    natsStatus s;

    NatsSubscriptionSync(natsSubscription* sub) //
        : ptr(sub)
    {
    }

    ~NatsSubscriptionSync()
    {
        if (ptr)
        {
            natsSubscription_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Disable copy
    NatsSubscriptionSync(const NatsSubscriptionSync&) = delete;
    NatsSubscriptionSync& operator=(const NatsSubscriptionSync&) = delete;

    // Enable move
    NatsSubscriptionSync(NatsSubscriptionSync&& other) noexcept : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }
    NatsSubscriptionSync& operator=(NatsSubscriptionSync&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                natsSubscription_Destroy(ptr);
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    expected<NatsMessageView, NatsError> next_msg(int64_t timeout_ms) noexcept
    {
        natsMsg* msg{nullptr};

        if ((s = natsSubscription_NextMsg(&msg, ptr, timeout_ms)) != NATS_OK)
            return std::unexpected(NatsError(s, "Failed to get next message from subscription."));

        return NatsMessageView(msg);
    }

    int64_t get_id() const noexcept
    {
        return natsSubscription_GetID(ptr);
    }

    expected<void, NatsError> no_delivery_delay() noexcept
    {
        if ((s = natsSubscription_NoDeliveryDelay(ptr)) != NATS_OK)
            return std::unexpected(NatsError(s, "Failed to set no delivery delay."));
        return {}; // Success
    }

    expected<uint64_t, NatsError> queued_msgs() noexcept
    {
        uint64_t queuedMsgs{0};
        if ((s = natsSubscription_QueuedMsgs(ptr, &queuedMsgs)) != NATS_OK)
            return std::unexpected(NatsError(s, "Failed to get queued messages count."));
        return queuedMsgs;
    }

    string_view subject() const noexcept
    {
        const char* str = natsSubscription_GetSubject(ptr);
        return string_view(str);
    }

    bool is_valid() const noexcept
    {
        return natsSubscription_IsValid(ptr);
    }

    expected<void, NatsError> auto_unsunscribe(int max) noexcept
    {
        if ((s = natsSubscription_AutoUnsubscribe(ptr, max)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s,
                std::format(
                    "Failed to set auto unsubscribe to {} for subscription [{}].", max, subject()
                )
            ));
        }
        return {}; // Success
    }

    expected<void, NatsError> drain() noexcept
    {
        if ((s = natsSubscription_Drain(ptr)) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to drain subscription [{}].", subject()))
            );
        }
        return {}; // Success
    }

    expected<void, NatsError> drain_timeout(int64_t timeout_ms) noexcept
    {
        if ((s = natsSubscription_DrainTimeout(ptr, timeout_ms)) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to drain subscription [{}].", subject()))
            );
        }
        return {}; // Success
    }

    expected<void, NatsError> wait_for_drain_completion(int64_t timeout_ms) noexcept
    {
        if ((s = natsSubscription_WaitForDrainCompletion(ptr, timeout_ms)) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to wait for drain completion [{}].", subject()))
            );
        }
        return {}; // Success
    }

    natsStatus drain_completion_status() const noexcept
    {
        return natsSubscription_DrainCompletionStatus(ptr);
    }

    struct MaxPending
    {
        int msgs{0};
        int bytes{0};
    };

    expected<MaxPending, NatsError> get_max_pending() noexcept
    {
        MaxPending mp;
        if ((s = natsSubscription_GetMaxPending(ptr, &mp.msgs, &mp.bytes)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s, std::format("Failed to get max pending stats for subscription [{}].", subject())
            ));
        }
        return mp;
    }

    expected<int64_t, NatsError> get_dropped() noexcept
    {
        int64_t count{0};
        if ((s = natsSubscription_GetDropped(ptr, &count)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s, std::format("Failed to get dropped count for subscription [{}].", subject())
            ));
        }
        return count;
    }

    expected<int64_t, NatsError> get_delivered() noexcept
    {
        int64_t count{0};
        if ((s = natsSubscription_GetDelivered(ptr, &count)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s, std::format("Failed to get delivered count for subscription [{}].", subject())
            ));
        }
        return count;
    }

    struct Pending
    {
        int msgs{0};
        int bytes{0};
    };

    expected<Pending, NatsError> get_pending() noexcept
    {
        Pending p;
        if ((s = natsSubscription_GetPending(ptr, &p.msgs, &p.bytes)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s, std::format("Failed to get pending stats for subscription [{}].", subject())
            ));
        }
        return p;
    }

    struct PendingLimits
    {
        int msgs{0};
        int bytes{0};
    };

    expected<PendingLimits, NatsError> get_pending_limits() noexcept
    {
        PendingLimits p;
        if ((s = natsSubscription_GetPendingLimits(ptr, &p.msgs, &p.bytes)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s,
                std::format("Failed to get pending limits stats for subscription [{}].", subject())
            ));
        }
        return p;
    }

    expected<void, NatsError> set_pending_limits(int msgs, int bytes) noexcept
    {
        if ((s = natsSubscription_SetPendingLimits(ptr, msgs, bytes)) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s,
                std::format(
                    "Failed to set pending limits for subscription [{}] to {} msgs and {} bytes.",
                    subject(),
                    msgs,
                    bytes
                )
            ));
        }
        return {}; // Success
    }
};
} // namespace nats
