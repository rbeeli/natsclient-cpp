#include <iostream>
#include <string>
#include <format>
#include <expected>

#include "nats_client/Client.hpp"

using std::string;
using std::expected;
using std::unexpected;

expected<void, nats::NatsError> run()
{
    auto res0 = nats::NatsClient::create();
    if (!res0)
        return unexpected(res0.error());
    nats::NatsClient& client = res0.value();

    client
        .options()                        //
        .set_url("nats://localhost:4222") //
        .set_send_asap(true);

    // Connect to NATS
    auto res = client.connect();
    if (!res)
        return unexpected(res.error());

    // Enable JetStream
    auto res2 = client.jet_stream();
    if (!res2)
        return unexpected(res2.error());

    std::cout << "Max payload size: " << client.get_max_payload() / 1024 << " KB\n";


    // auto res_test = client.subscribe_sync("test");
    // if (!res_test)
    //     return unexpected(res_test.error());

    // nats::NatsSubscriptionSync& sub = res_test.value();
    // sub.no_delivery_delay();

    // while (true)
    // {
    //     auto res_msg = sub.next_msg(1000);
    //     if (!res_msg)
    //     {
    //         if (res_msg.error().status != NATS_TIMEOUT)
    //             return unexpected(res_msg.error());
    //
    //         std::cout << "Timeout, waiting for next message...\n";
    //     }
    //     else
    //     {
    //         nats::NatsMessageView& msg = res_msg.value();
    //         std::cout << std::format(
    //             "Received message: {} with {} bytes\n", msg.data(), msg.data_length()
    //         );
    //     }
    // }


    for (int i = 0; i < 1000; ++i)
    {
        string payload = std::format("Hello, World! {}", i + 1);
        auto res_pub = client.publish("test", payload);
        if (!res_pub)
            return unexpected(res_pub.error());
        if (i % 100 == 0)
            std::cout << std::format("Published message: {}\n", i);
    }


    // // Bind to existing KeyValue store
    // auto res3 = client.kvs_bind("my-kv");
    // if (!res3)
    //     return unexpected(res3.error());
    // nats::KvStore& kv = res3.value();


    // // Get KV keys
    // auto res6 = client.kvs_keys(kv, &opts);
    // if (!res6)
    //     return unexpected(res6.error());
    // nats::KvKeysList& keys = res6.value();
    // std::cout << std::format("Keys in KV store [{}]:\n", kv.bucket());
    // for (const string& key : keys.keys())
    // {
    //     std::cout << std::format("- {}\n", key);
    // }

    // // Create new KV bucket
    // string bucket = "test_cpp";
    // auto res8 = client.kvs_create(bucket);
    // if (!res8)
    //     return unexpected(res8.error());


    // // Create watch options
    // auto res4 = client.kvs_watch_options();
    // if (!res4)
    //     return unexpected(res4.error());
    // kvWatchOptions& opts = res4.value();

    // // Create watcher
    // // auto res5 = client.kvs_watch(kv, "*", &opts);
    // auto res5 = client.kvs_watch(kv, "*", NULL);
    // if (!res5)
    //     return unexpected(res5.error());
    // nats::KvWatcher& watcher = res5.value();

    // bool init = false;
    // while (true)
    // {
    //     auto res7 = watcher.next(5000);
    //     if (!res7)
    //     {
    //         if (res7.error().status == NATS_TIMEOUT)
    //         {
    //             std::cout << "Timeout, waiting for next update...\n";
    //             continue;
    //         }
    //         return unexpected(res7.error());
    //     }

    //     if ((*res7).has_value())
    //     {
    //         if (!init)
    //             std::cout << "Initial state: ";
    //         nats::KvEntry& e = (*res7).value();
    //         std::cout << std::format("Key: {}, Value: {}\n", e.key(), e.value_string());
    //     }
    //     else if (!init)
    //     {
    //         // got all entries during initialization
    //         init = true;
    //     }
    // }

    return {}; // Success
}

int main()
{
    auto ret = run();

    nats_Close();

    if (!ret)
    {
        std::cerr << ret.error().to_string() << std::endl;
        return 1;
    }

    return 0;
}
