#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <expected>
#include <nats/nats.h>

namespace nats
{
using std::string;
using std::string_view;
using std::vector;
using std::span;
using std::byte;
using std::optional;
using std::expected;
using std::unexpected;

struct KvStore
{
    kvStore* ptr = nullptr;

    KvStore(kvStore* kv_store) noexcept //
        : ptr(kv_store)
    {
    }

    ~KvStore() noexcept
    {
        if (ptr)
        {
            kvStore_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Delete copy constructor and assignment operator
    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;

    // Allow moving
    KvStore(KvStore&& other) noexcept : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    KvStore& operator=(KvStore&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                kvStore_Destroy(ptr);
                ptr = nullptr;
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    string_view bucket() const noexcept
    {
        const char* name = kvStore_Bucket(ptr);
        return string_view(name);
    }
};

struct KvEntry
{
    kvEntry* ptr = nullptr;

    KvEntry(kvEntry* kv_entry) noexcept //
        : ptr(kv_entry)
    {
    }

    ~KvEntry() noexcept
    {
        if (ptr)
        {
            kvEntry_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Delete copy constructor and assignment operator
    KvEntry(const KvEntry&) = delete;
    KvEntry& operator=(const KvEntry&) = delete;

    // Allow moving
    KvEntry(KvEntry&& other) noexcept : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    KvEntry& operator=(KvEntry&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                kvEntry_Destroy(ptr);
                ptr = nullptr;
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    string_view bucket() const noexcept
    {
        return kvEntry_Bucket(ptr);
    }

    string_view key() const noexcept
    {
        return kvEntry_Key(ptr);
    }

    string_view value_string() const noexcept
    {
        return kvEntry_ValueString(ptr);
    }

    size_t value_len() const noexcept
    {
        return kvEntry_ValueLen(ptr);
    }

    span<const byte> value_bytes() const noexcept
    {
        const void* data = kvEntry_Value(ptr);
        size_t size = value_len();
        return {static_cast<const byte*>(data), size};
    }

    uint64_t revision() const noexcept
    {
        return kvEntry_Revision(ptr);
    }

    int64_t created() const noexcept
    {
        return kvEntry_Created(ptr);
    }

    uint64_t delta() const noexcept
    {
        return kvEntry_Delta(ptr);
    }

    kvOperation operation() const noexcept
    {
        return kvEntry_Operation(ptr);
    }
};

struct KvWatcher
{
    kvWatcher* ptr = nullptr;
    natsStatus s;

    KvWatcher(kvWatcher* kv_watcher) noexcept //
        : ptr(kv_watcher)
    {
    }

    ~KvWatcher() noexcept
    {
        if (ptr)
        {
            kvWatcher_Destroy(ptr);
            ptr = nullptr;
        }
    }

    // Delete copy constructor and assignment operator
    KvWatcher(const KvWatcher&) = delete;
    KvWatcher& operator=(const KvWatcher&) = delete;

    // Allow moving
    KvWatcher(KvWatcher&& other) noexcept : ptr(other.ptr)
    {
        other.ptr = nullptr;
    }

    KvWatcher& operator=(KvWatcher&& other) noexcept
    {
        if (this != &other)
        {
            if (ptr)
            {
                kvWatcher_Destroy(ptr);
                ptr = nullptr;
            }
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /**
     * Returns the next entry for the given watcher.
     *
     * The entry may be `std::nullopt` (no error) to indicate that the
     * initial state has been retrieved.
     * After that, the function will block until a new entry is available,
     * or the timeout is reached.
     */
    expected<optional<KvEntry>, NatsError> next(int64_t timeout_ms) noexcept
    {
        kvEntry* e = NULL;

        if ((s = kvWatcher_Next(&e, ptr, timeout_ms)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to get next KV watcher update."));

        if (e == NULL)
            return std::nullopt;

        return KvEntry(e);
    }

    /**
     * Stops the watcher.
     *
     * After this call, new and existing calls to `next`
     * (that are waiting for an update) will return error with `NATS_ILLEGAL_STATE`.
     */
    expected<void, NatsError> stop() noexcept
    {
        if ((s = kvWatcher_Stop(ptr)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to stop KV watcher."));

        return {};
    }
};

struct KvKeysList
{
    kvKeysList kl{};

    KvKeysList() noexcept
    {
    }

    ~KvKeysList() noexcept
    {
        if (kl.Keys != NULL)
        {
            kvKeysList_Destroy(&kl);
            kl.Keys = NULL;
            kl.Count = 0;
        }
    }

    // Delete copy constructor and assignment operator
    KvKeysList(const KvKeysList&) = delete;
    KvKeysList& operator=(const KvKeysList&) = delete;

    // Allow moving
    KvKeysList(KvKeysList&& other) noexcept : kl(other.kl)
    {
        other.kl.Keys = NULL;
        other.kl.Count = 0;
    }
    KvKeysList& operator=(KvKeysList&& other) noexcept
    {
        if (this != &other)
        {
            if (kl.Keys != NULL)
            {
                kvKeysList_Destroy(&kl);
                kl.Keys = NULL;
                kl.Count = 0;
            }
            kl = other.kl;
            other.kl.Keys = NULL;
            other.kl.Count = 0;
        }
        return *this;
    }

    vector<string> keys() const noexcept
    {
        vector<string> keys;
        for (int i = 0; i < kl.Count; i++)
            keys.push_back(kl.Keys[i]);
        return keys;
    }
};
} // namespace nats
