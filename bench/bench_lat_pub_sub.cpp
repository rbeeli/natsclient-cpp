#include <iostream>
#include <cstddef>
#include <string>
#include <format>
#include <expected>
#include <thread>
#include <chrono>
#include <span>
#include <time.h>

#include "nats_client/Client.hpp"

using std::string;
using std::expected;
using std::unexpected;
using std::span;
using std::byte;
using namespace std::chrono;

const char* subject = "bench_latency";

int64_t nanos() noexcept
{
    auto now = high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return duration_cast<nanoseconds>(duration).count();
}

expected<void, nats::NatsError> run_producer()
{
    auto res0 = nats::NatsClient::create();
    if (!res0)
        return unexpected(res0.error());
    nats::NatsClient& client = res0.value();

    client
        .options()                        //
        .set_url("nats://localhost:4222") //
        .set_send_asap(true) //
        ;

    auto res = client.connect();
    if (!res)
        return unexpected(res.error());

    std::cout << "Max payload size: " << client.get_max_payload() / 1024 << " KB\n";

    // string payload = "Hello, World!";
    for (int i = 0; i < 100'000'000; ++i)
    {
        auto nanoseconds = nanos();
        span<const byte> payload{reinterpret_cast<const byte*>(&nanoseconds), sizeof(nanoseconds)};
        auto res_pub = client.publish(subject, payload);
        if (!res_pub)
            return unexpected(res_pub.error());
        // if (i % 100 == 0)
        //     std::cout << std::format("Published message: {}\n", i);

        // nats_Sleep(0);
        // std::this_thread::sleep_for(std::chrono::nanoseconds(1'000));
        struct timespec ts = {0, 1'000}; // 1 us
        nanosleep(&ts, NULL);
    }

    return {}; // Success
}

expected<void, nats::NatsError> run_consumer()
{
    auto res0 = nats::NatsClient::create();
    if (!res0)
        return unexpected(res0.error());
    nats::NatsClient& client = res0.value();

    client
        .options()                        //
        .set_url("nats://localhost:4222") //
        .set_send_asap(true);

    auto res = client.connect();
    if (!res)
        return unexpected(res.error());

    auto res_test = client.subscribe_sync(subject);
    if (!res_test)
        return unexpected(res_test.error());

    nats::NatsSubscriptionSync& sub = res_test.value();
    // sub.no_delivery_delay();

    int64_t counter = 0;
    int64_t latency_sum = 0;
    auto last_time = high_resolution_clock::now();
    while (true)
    {
        auto res_msg = sub.next_msg(99999999);
        auto ns = nanos();
        if (!res_msg)
            return unexpected(res_msg.error());

        ++counter;

        int64_t msg_ts = *reinterpret_cast<const int64_t*>(res_msg.value().data().data());
        int64_t latency = ns - msg_ts;
        latency_sum += latency;

        auto time = high_resolution_clock::now();
        if (duration_cast<seconds>(time - last_time).count() >= 1)
        {
            auto throughput = counter / duration_cast<seconds>(time - last_time).count();
            auto avg_latency = latency_sum / counter;
            std::cout << std::format("{} msgs/s, avg. latency {} ns\n", throughput, avg_latency);
            counter = 0;
            latency_sum = 0;
            last_time = time;
        }
    }

    return {}; // Success
}

int main()
{
    // run producer in thread
    std::jthread producer_thread(
        []()
        {
            auto ret = run_producer();
            if (!ret)
            {
                std::cerr << ret.error().to_string() << std::endl;
            }
        }
    );

    // run consumer in thread
    std::jthread consumer_thread(
        []()
        {
            auto ret = run_consumer();
            if (!ret)
            {
                std::cerr << ret.error().to_string() << std::endl;
            }
        }
    );

    producer_thread.join();
    consumer_thread.join();

    std::cout << "Done\n";

    nats_Close();

    return 0;
}
