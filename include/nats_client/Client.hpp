#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <iostream>
#include <span>
#include <expected>
#include <vector>
#include <format>

#include <nats/nats.h>
#include "Options.hpp"
#include "Error.hpp"
#include "Kv.hpp"
#include "SubscriptionSync.hpp"

// KV store API: https://docs.nats.io/using-nats/developer/develop_jetstream/kv

namespace nats
{
using std::string;
using std::string_view;
using std::expected;
using std::unexpected;
using std::byte;
using std::span;
using std::vector;

class NatsClient
{
private:
    natsConnection* conn = nullptr;
    NatsOptions opts;
    jsCtx* js = NULL;
    natsStatus s;
    jsOptions jsOpts;

    void cleanup()
    {
        if (conn)
        {
            natsConnection_Destroy(conn);
            conn = nullptr;
        }

        opts.~NatsOptions();

        if (js)
        {
            jsCtx_Destroy(js);
            js = NULL;
        }
        // nats_Close(); // there could be other clients
    }

    NatsClient(NatsOptions&& opts) noexcept //
        : opts(std::move(opts))
    {
    }

public:
    ~NatsClient()
    {
        cleanup();
    }

    static expected<NatsClient, NatsError> create() noexcept
    {
        natsOptions* opts = nullptr;

        // Create global options
        auto s = natsOptions_Create(&opts);
        if (s != NATS_OK)
            return unexpected(NatsError(s, "Error creating NATS options struct."));

        return std::expected<NatsClient, NatsError>(NatsClient(NatsOptions(opts)));
    }

    // Delete copy constructor and assignment operator
    NatsClient(const NatsClient&) = delete;
    NatsClient& operator=(const NatsClient&) = delete;

    // Allow moving
    NatsClient(NatsClient&& other) noexcept : conn(other.conn), opts(std::move(other.opts))
    {
        other.conn = nullptr;
    }
    NatsClient& operator=(NatsClient&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            conn = other.conn;
            opts = std::move(other.opts);
            other.conn = nullptr;
            other.opts = nullptr;
        }
        return *this;
    }

    /**
     * Returns the underlying NATS connection C object pointer.
     */
    natsConnection* connection() const noexcept
    {
        return conn;
    }

    /**
     * Returns the options object of the NATS connection.
     *
     * This object is used when one wants to set
     * specific options PRIOR to connecting to the NATS server.
     *
     * Do all the needed `set_XXX` calls before calling the `connect` method.
     * After calling `connect`, modifications to the options object will not affect the connection.
     */
    NatsOptions& options() noexcept
    {
        return opts;
    }

    /**
     * Connect to NATS server.
     * To enable JetStream, call `jet_stream()` after connecting.
     */
    expected<void, NatsError> connect() noexcept
    {
        if (opts.s != NATS_OK)
            return unexpected(NatsError(opts.s, "NATS options has an error."));

        // Set disconnected callback
        if ((s = natsOptions_SetDisconnectedCB(opts.ptr, disconnected_callback, NULL)) != NATS_OK)
            return unexpected(NatsError(s, "Error setting disconnected callback."));

        // Set reconnected callback
        if ((s = natsOptions_SetReconnectedCB(opts.ptr, reconnected_callback, NULL)) != NATS_OK)
            return unexpected(NatsError(s, "Error setting reconnected callback."));

        // Set closed callback
        if ((s = natsOptions_SetClosedCB(opts.ptr, closed_callback, NULL)) != NATS_OK)
            return unexpected(NatsError(s, "Error setting closed callback."));

        // Set error handler
        if ((s = natsOptions_SetErrorHandler(opts.ptr, error_handler_callback, nullptr)) != NATS_OK)
            return unexpected(NatsError(s, "Error setting error handler callback."));

        // Connect
        if ((s = natsConnection_Connect(&conn, opts.ptr)) != NATS_OK)
            return unexpected(NatsError(s, "Connect failed. Check NATS server is running."));

        return {}; // Success
    }

    /**
     * Enable JetStream for the connection.
     */
    expected<void, NatsError> jet_stream() noexcept
    {
        // Initialize and set some JetStream options
        if ((s = jsOptions_Init(&jsOpts)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to initialize JetStream options."));

        // Set some options TODO
        jsOpts.PublishAsync.MaxPending = 256;

        // Create JetStream Context
        if ((s = natsConnection_JetStream(&js, conn, &jsOpts)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to create JetStream context."));

        return {}; // Success
    }

    /**
     * Returns the maximum payload size that can be sent to the server.
     */
    int64_t get_max_payload() noexcept
    {
        return natsConnection_GetMaxPayload(conn);
    }

    /**
     * Initializes a KeyValue configuration structure.
     */
    expected<kvConfig, NatsError> kvs_config_init() noexcept
    {
        kvConfig cfg;
        if ((s = kvConfig_Init(&cfg)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to initialize KV config."));
        return cfg;
    }

    /**
     * Creates a KeyValue store with a given configuration.
     *
     * Bucket names are restricted to this set of characters: `A-Z`, `a-z`, `0-9`, `_` and `-`.
     */
    expected<KvStore, NatsError> kvs_create(
        string_view bucket_name, optional<string_view> description = std::nullopt
    ) noexcept
    {
        if (bucket_name.empty())
            return unexpected(NatsError(NATS_INVALID_ARG, "Bucket name is required."));

        auto res_cfg = kvs_config_init();
        if (!res_cfg)
            return unexpected(res_cfg.error());

        kvConfig& config = res_cfg.value();
        config.Bucket = bucket_name.data();

        kvStore* kv = NULL;
        if ((s = js_CreateKeyValue(&kv, js, &config)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to create KV bucket."));
        return KvStore(kv);
    }

    /**
     * Looks-up and binds to an existing KeyValue store.
     */
    expected<KvStore, NatsError> kvs_bind(string_view bucket) noexcept
    {
        kvStore* kv = NULL;
        if ((s = js_KeyValue(&kv, js, bucket.data())) != NATS_OK)
        {
            return unexpected(NatsError(s, std::format("Failed to bind to KV bucket [{}].", bucket))
            );
        }
        return KvStore(kv);
    }

    /**
     * Deletes a KeyValue store.
     */
    expected<void, NatsError> kvs_delete(string_view bucket) noexcept
    {
        if ((s = js_DeleteKeyValue(js, bucket.data())) != NATS_OK)
        {
            return unexpected(NatsError(s, std::format("Failed to delete KV bucket [{}].", bucket))
            );
        }
        return {}; // Success
    }

    /**
     * Returns all keys in the bucket.
     */
    expected<KvKeysList, NatsError> kvs_keys(KvStore& kv_store, kvWatchOptions* opts) noexcept
    {
        KvKeysList list;
        if ((s = kvStore_Keys(&list.kl, kv_store.ptr, opts)) != NATS_OK)
        {
            return unexpected(NatsError(
                s, std::format("Failed to get keys for KV bucket [{}].", kv_store.bucket())
            ));
        }
        return list;
    }

    /**
     * Initializes a KeyValue watcher options structure.
     *
     * Use this before setting specific watcher options and passing it
     * to `kvs_watch`.
     */
    expected<kvWatchOptions, NatsError> kvs_watch_options() noexcept
    {
        kvWatchOptions o;
        if ((s = kvWatchOptions_Init(&o)) != NATS_OK)
            return unexpected(NatsError(s, "Failed to initialize KV watch options."));
        return o;
    }

    /**
     * Returns a watcher for any updates to keys that match the `keys` argument,
     * which could include wildcard.
     */
    expected<KvWatcher, NatsError> kvs_watch(
        KvStore& kv_store, string_view key, kvWatchOptions* opts
    ) noexcept
    {
        kvWatcher* w = NULL;
        if ((s = kvStore_Watch(&w, kv_store.ptr, key.data(), opts)) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to create KV watcher for key [{}] in bucket [{}].",
                    key,
                    kv_store.bucket()
                )
            ));
        }
        return KvWatcher(w);
    }

    /**
     * Returns the latest entry for the key.
     */
    expected<KvEntry, NatsError> kv_get(KvStore& kv_store, string_view key) noexcept
    {
        kvEntry* e = NULL;
        if ((s = kvStore_Get(&e, kv_store.ptr, key.data())) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to read KV entry [{}] from bucket [{}].", key, kv_store.bucket()
                )
            ));
        }
        return KvEntry(e);
    }

    /**
     * Places the value for the key into the store if and only if the key does not exist.
     */
    expected<void, NatsError> kv_create(
        KvStore& kv_store, string_view key, span<byte> data
    ) noexcept
    {
        uint64_t* rev = NULL;
        size_t size = data.size_bytes();
        if ((s = kvStore_Create(rev, kv_store.ptr, key.data(), data.data(), size)) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to create KV entry for key [{}] with {} bytes in bucket [{}].",
                    key,
                    data.size_bytes(),
                    kv_store.bucket()
                )
            ));
        }
        return {}; // Success
    }

    /**
     * Places the value (as a string) for the key into the store if and only if the key does not exist.
     */
    expected<void, NatsError> kv_create_string(
        KvStore& kv_store, string_view key, string_view data
    ) noexcept
    {
        uint64_t* rev = NULL;
        if ((s = kvStore_CreateString(rev, kv_store.ptr, key.data(), data.data())) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to create KV string entry for key [{}] in bucket [{}].",
                    key,
                    kv_store.bucket()
                )
            ));
        }
        return {}; // Success
    }

    /**
     * Places the new value (as a string) for the key into the store.
     */
    expected<void, NatsError> kv_put_string(
        KvStore& kv_store, string_view key, string_view value
    ) noexcept
    {
        if ((s = kvStore_PutString(NULL, kv_store.ptr, key.data(), value.data())) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to put KV string with key [{}] and {} bytes in bucket [{}].",
                    key,
                    value.size(),
                    kv_store.bucket()
                )
            ));
        }
        return {}; // Success
    }

    /**
     * Places the new value for the key into the store.
     */
    expected<void, NatsError> kv_put(KvStore& kv_store, string_view key, span<byte> data) noexcept
    {
        if ((s = kvStore_Put(NULL, kv_store.ptr, key.data(), data.data(), data.size_bytes())) !=
            NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format(
                    "Failed to put KV data with key [{}] and {} bytes in bucket [{}].",
                    key,
                    data.size_bytes(),
                    kv_store.bucket()
                )
            ));
        }
        return {}; // Success
    }

    /**
     * Deletes a key by placing a delete marker and leaving all revisions.
     */
    expected<void, NatsError> kv_delete(KvStore& kv_store, string_view key) noexcept
    {
        if ((s = kvStore_Delete(kv_store.ptr, key.data())) != NATS_OK)
        {
            return unexpected(NatsError(
                s,
                std::format("Failed to delete KV key [{}] in bucket [{}].", key, kv_store.bucket())
            ));
        }
        return {}; // Success
    }

    /**
     * Creates a synchronous subcription which requires manual polling.
     */
    expected<NatsSubscriptionSync, NatsError> subscribe_sync(const string_view subject) noexcept
    {
        natsSubscription* sub = nullptr;
        if ((s = natsConnection_SubscribeSync(&sub, conn, subject.data())) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to subscribe to subject [{}].", subject))
            );
        }
        return NatsSubscriptionSync(sub);
    }

    /**
     * Creates a synchronous queue subcription which requires manual polling.
     */
    expected<NatsSubscriptionSync, NatsError> queue_subscribe_sync(
        string_view subject, string_view queue_group
    ) noexcept
    {
        natsSubscription* sub = nullptr;
        if ((s = natsConnection_QueueSubscribeSync(&sub, conn, subject.data(), queue_group.data())
            ) != NATS_OK)
        {
            return std::unexpected(NatsError(
                s,
                std::format(
                    "Failed to queue subscribe to subject [{}] with group [{}].",
                    subject,
                    queue_group
                )
            ));
        }
        return NatsSubscriptionSync(sub);
    }

    expected<void, NatsError> unsubscribe(NatsSubscriptionSync& sub) noexcept
    {
        if ((s = natsSubscription_Unsubscribe(sub.ptr)) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to unsubscribe from subject [{}].", sub.subject()))
            );
        }
        return {}; // Success
    }

    /**
     * Publishes a string on a subject.
     *
     * Convenient function to publish a string.
     */
    expected<void, NatsError> publish(string_view subject, string_view data) noexcept
    {
        if ((s = natsConnection_Publish(conn, subject.data(), data.data(), data.size())) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to publish string with {} bytes to subject [{}].", data.size(), subject))
            );
        }
        return {}; // Success
    }

    /**
     * Publishes the data argument to the given subject.
     *
     * The data argument is left untouched and needs to be
     * correctly interpreted on the receiver.
     */
    expected<void, NatsError> publish(string_view subject, span<const byte> data) noexcept
    {
        if ((s = natsConnection_Publish(conn, subject.data(), data.data(), data.size())) != NATS_OK)
        {
            return std::unexpected(
                NatsError(s, std::format("Failed to publish {} bytes to subject [{}].", data.size(), subject))
            );
        }
        return {}; // Success
    }

private:
    static void error_handler_callback(
        natsConnection* nc, natsSubscription* sub, natsStatus err, void* closure
    ) noexcept
    {
        std::cerr << std::format(
            "NatsClient async error: {} - {}\n", (int)err, natsStatus_GetText(err)
        );

        int64_t dropped;
        if (natsSubscription_GetDropped(sub, &dropped) == NATS_OK)
        {
            std::cerr << std::format("NatsClient dropped messages so far: {}\n", dropped);
        }
    }

    static void disconnected_callback(natsConnection* conn, void* closure) noexcept
    {
        std::cerr << "NatsClient disconnected\n";
    }

    static void reconnected_callback(natsConnection* conn, void* closure) noexcept
    {
        std::cerr << "NatsClient reconnected\n";
    }

    static void closed_callback(natsConnection* conn, void* closure) noexcept
    {
        std::cerr << "NatsClient connection closed\n";
    }
};

} // namespace nats
