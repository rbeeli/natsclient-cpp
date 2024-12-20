#include <iostream>
#include <cstddef>
#include <string>
#include <format>
#include <expected>
#include <thread>
#include <chrono>
#include <atomic>
#include <array>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <zmq.hpp>

using std::string;
using std::expected;
using std::unexpected;
using namespace std::chrono;

const char* shm_name = "/zmq_bench_shm";
const size_t shm_size = 4096;

struct SharedData {
    std::atomic<int64_t> timestamp;
    std::atomic<bool> ready;
    std::atomic<bool> should_exit;
};

inline int64_t nanos() noexcept
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void set_thread_priority(int priority)
{
    sched_param sch_params;
    sch_params.sched_priority = priority;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params) != 0) {
        std::cerr << "Failed to set thread priority\n";
    }
}

expected<void, string> run_producer(SharedData* shared_data)
{
    set_thread_priority(99);

    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::pub);

    try {
        socket.bind("inproc://trigger");
    } catch (const zmq::error_t& e) {
        return unexpected(std::format("Failed to bind socket: {}", e.what()));
    }

    std::cout << "Producer started\n";

    for (int i = 0; i < 10'000'000 && !shared_data->should_exit.load(std::memory_order_relaxed); ++i)
    {
        int64_t current_time = nanos();
        shared_data->timestamp.store(current_time, std::memory_order_release);
        shared_data->ready.store(true, std::memory_order_release);
        
        socket.send(zmq::str_buffer("trigger"), zmq::send_flags::dontwait);

        while (shared_data->ready.load(std::memory_order_acquire) && 
               !shared_data->should_exit.load(std::memory_order_relaxed)) {
            std::this_thread::yield();
        }
    }

    shared_data->should_exit.store(true, std::memory_order_relaxed);
    return {};
}

expected<void, string> run_consumer(SharedData* shared_data)
{
    set_thread_priority(99);

    zmq::context_t context(1);
    zmq::socket_t socket(context, zmq::socket_type::sub);

    try {
        socket.connect("inproc://trigger");
        socket.set(zmq::sockopt::subscribe, "");
    } catch (const zmq::error_t& e) {
        return unexpected(std::format("Failed to connect or subscribe: {}", e.what()));
    }

    std::cout << "Consumer started\n";

    int64_t counter = 0;
    int64_t latency_sum = 0;
    auto last_time = high_resolution_clock::now();

    while (!shared_data->should_exit.load(std::memory_order_relaxed))
    {
        zmq::message_t message;
        try {
            auto recv_result = socket.recv(message, zmq::recv_flags::dontwait);
            if (!recv_result) {
                std::this_thread::yield();
                continue;
            }
        } catch (const zmq::error_t& e) {
            return unexpected(std::format("Failed to receive message: {}", e.what()));
        }

        while (!shared_data->ready.load(std::memory_order_acquire) && 
               !shared_data->should_exit.load(std::memory_order_relaxed)) {
            std::this_thread::yield();
        }

        auto ns = nanos();
        int64_t msg_ts = shared_data->timestamp.load(std::memory_order_acquire);
        shared_data->ready.store(false, std::memory_order_release);

        ++counter;
        int64_t latency = ns - msg_ts;
        latency_sum += latency;

        auto time = high_resolution_clock::now();
        if (duration_cast<seconds>(time - last_time).count() >= 1)
        {
            auto duration = duration_cast<seconds>(time - last_time).count();
            auto throughput = counter / duration;
            auto avg_latency = latency_sum / counter;
            std::cout << std::format("{} msgs/s, avg. latency {} ns\n", throughput, avg_latency);
            counter = 0;
            latency_sum = 0;
            last_time = time;
        }
    }

    return {};
}

int main()
{
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::cerr << "Failed to create shared memory\n";
        return 1;
    }

    if (ftruncate(shm_fd, shm_size) == -1) {
        std::cerr << "Failed to set shared memory size\n";
        return 1;
    }

    void* shm_ptr = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        std::cerr << "Failed to map shared memory\n";
        return 1;
    }

    SharedData* shared_data = new (shm_ptr) SharedData();
    shared_data->ready.store(false);
    shared_data->should_exit.store(false);

    std::jthread producer_thread(
        [shared_data]()
        {
            auto ret = run_producer(shared_data);
            if (!ret)
            {
                std::cerr << ret.error() << std::endl;
            }
        }
    );

    std::jthread consumer_thread(
        [shared_data]()
        {
            auto ret = run_consumer(shared_data);
            if (!ret)
            {
                std::cerr << ret.error() << std::endl;
            }
        }
    );

    producer_thread.join();
    consumer_thread.join();

    std::cout << "Done\n";

    munmap(shm_ptr, shm_size);
    shm_unlink(shm_name);

    return 0;
}















// #include <iostream>
// #include <cstddef>
// #include <string>
// #include <format>
// #include <expected>
// #include <thread>
// #include <chrono>
// #include <span>
// #include <time.h>
// #include <zmq.hpp>
// #include <sched.h>

// using std::string;
// using std::expected;
// using std::unexpected;
// using std::span;
// using std::byte;
// using namespace std::chrono;

// const char* ipc_address = "ipc:///tmp/zmq_bench";

// inline int64_t nanos() noexcept
// {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return ts.tv_sec * 1000000000 + ts.tv_nsec;
// }

// void set_thread_priority(int priority)
// {
//     sched_param sch_params;
//     sch_params.sched_priority = priority;
//     if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch_params) != 0) {
//         std::cerr << "Failed to set thread priority\n";
//     }
// }

// inline void short_pause() noexcept
// {
//     std::this_thread::yield();
// }

// expected<void, string> run_producer()
// {
//     set_thread_priority(99); // Set to maximum real-time priority

//     zmq::context_t context(1);
//     zmq::socket_t socket(context, zmq::socket_type::pub);

//     // Set socket options for low latency
//     int send_hwm = 1000;
//     socket.set(zmq::sockopt::sndhwm, send_hwm);
//     int linger = 0;
//     socket.set(zmq::sockopt::linger, linger);

//     try {
//         socket.bind(ipc_address);
//     } catch (const zmq::error_t& e) {
//         return unexpected(std::format("Failed to bind socket: {}", e.what()));
//     }

//     std::cout << "Producer started\n";

//     for (int i = 0; i < 100'000'000; ++i)
//     {
//         int64_t nanoseconds = nanos();
//         zmq::message_t message(&nanoseconds, sizeof(nanoseconds));
        
//         try {
//             socket.send(message, zmq::send_flags::dontwait);
//         } catch (const zmq::error_t& e) {
//             return unexpected(std::format("Failed to send message: {}", e.what()));
//         }

//         short_pause(); // Short pause
//     }

//     return {};
// }

// expected<void, string> run_consumer()
// {
//     set_thread_priority(99); // Set to maximum real-time priority

//     zmq::context_t context(1);
//     zmq::socket_t socket(context, zmq::socket_type::sub);

//     // Set socket options for low latency
//     int rcv_hwm = 1000;
//     socket.set(zmq::sockopt::rcvhwm, rcv_hwm);
//     int linger = 0;
//     socket.set(zmq::sockopt::linger, linger);

//     try {
//         socket.connect(ipc_address);
//         socket.set(zmq::sockopt::subscribe, "");
//     } catch (const zmq::error_t& e) {
//         return unexpected(std::format("Failed to connect or subscribe: {}", e.what()));
//     }

//     std::cout << "Consumer started\n";

//     int64_t counter = 0;
//     int64_t latency_sum = 0;
//     auto last_time = high_resolution_clock::now();

//     while (true)
//     {
//         zmq::message_t message;
//         try {
//             auto recv_result = socket.recv(message, zmq::recv_flags::dontwait);
//             if (!recv_result) {
//                 // short_pause(); // Short pause
//                 continue;
//             }
//         } catch (const zmq::error_t& e) {
//             return unexpected(std::format("Failed to receive message: {}", e.what()));
//         }

//         auto ns = nanos();
//         ++counter;

//         int64_t msg_ts = *reinterpret_cast<const int64_t*>(message.data());
//         int64_t latency = ns - msg_ts;
//         latency_sum += latency;

//         auto time = high_resolution_clock::now();
//         if (duration_cast<seconds>(time - last_time).count() >= 1)
//         {
//             auto throughput = counter / duration_cast<seconds>(time - last_time).count();
//             auto avg_latency = latency_sum / counter;
//             std::cout << std::format("{} msgs/s, avg. latency {} ns\n", throughput, avg_latency);
//             counter = 0;
//             latency_sum = 0;
//             last_time = time;
//         }
//     }

//     return {};
// }

// int main()
// {
//     std::jthread producer_thread(
//         []()
//         {
//             auto ret = run_producer();
//             if (!ret)
//             {
//                 std::cerr << ret.error() << std::endl;
//             }
//         }
//     );

//     std::jthread consumer_thread(
//         []()
//         {
//             auto ret = run_consumer();
//             if (!ret)
//             {
//                 std::cerr << ret.error() << std::endl;
//             }
//         }
//     );

//     producer_thread.join();
//     consumer_thread.join();

//     std::cout << "Done\n";

//     return 0;
// }






















// #include <iostream>
// #include <cstddef>
// #include <string>
// #include <format>
// #include <expected>
// #include <thread>
// #include <chrono>
// #include <span>
// #include <time.h>
// #include <zmq.hpp>

// using std::string;
// using std::expected;
// using std::unexpected;
// using std::span;
// using std::byte;
// using namespace std::chrono;

// const char* ipc_address = "ipc:///tmp/zmq_bench";

// int64_t nanos() noexcept
// {
//     auto now = high_resolution_clock::now();
//     auto duration = now.time_since_epoch();
//     return duration_cast<nanoseconds>(duration).count();
// }

// expected<void, string> run_producer()
// {
//     zmq::context_t context(1);
//     zmq::socket_t socket(context, zmq::socket_type::pub);

//     try {
//         socket.bind(ipc_address);
//     } catch (const zmq::error_t& e) {
//         return unexpected(std::format("Failed to bind socket: {}", e.what()));
//     }

//     std::cout << "Producer started\n";

//     for (int i = 0; i < 1'000'000'000; ++i)
//     {
//         auto nanoseconds = nanos();
//         zmq::message_t message(&nanoseconds, sizeof(nanoseconds));
        
//         try {
//             socket.send(message, zmq::send_flags::none);
//         } catch (const zmq::error_t& e) {
//             return unexpected(std::format("Failed to send message: {}", e.what()));
//         }

//         // struct timespec ts = {0, 1'000}; // 1 us
//         // nanosleep(&ts, NULL);
//     }

//     std::cout << "Producer done\n";

//     return {};
// }

// expected<void, string> run_consumer()
// {
//     zmq::context_t context(1);
//     zmq::socket_t socket(context, zmq::socket_type::sub);

//     try {
//         socket.connect(ipc_address);
//         socket.set(zmq::sockopt::subscribe, "");
//     } catch (const zmq::error_t& e) {
//         return unexpected(std::format("Failed to connect or subscribe: {}", e.what()));
//     }

//     std::cout << "Consumer started\n";

//     int64_t counter = 0;
//     int64_t latency_sum = 0;
//     auto last_time = high_resolution_clock::now();

//     while (true)
//     {
//         zmq::message_t message;
//         try {
//             auto recv_result = socket.recv(message, zmq::recv_flags::none);
//             if (!recv_result) {
//                 continue;
//             }
//         } catch (const zmq::error_t& e) {
//             return unexpected(std::format("Failed to receive message: {}", e.what()));
//         }

//         auto ns = nanos();
//         ++counter;

//         int64_t msg_ts = *reinterpret_cast<const int64_t*>(message.data());
//         int64_t latency = ns - msg_ts;
//         latency_sum += latency;

//         auto time = high_resolution_clock::now();
//         if (duration_cast<seconds>(time - last_time).count() >= 1)
//         {
//             auto throughput = counter / duration_cast<seconds>(time - last_time).count();
//             auto avg_latency = latency_sum / counter;
//             std::cout << std::format("{} msgs/s, avg. latency {} ns\n", throughput, avg_latency);
//             counter = 0;
//             latency_sum = 0;
//             last_time = time;
//         }
//     }

//     std::cout << "Consumer done\n";

//     return {};
// }

// int main()
// {
//     std::jthread producer_thread(
//         []()
//         {
//             auto ret = run_producer();
//             if (!ret)
//             {
//                 std::cerr << ret.error() << std::endl;
//             }
//         }
//     );

//     std::jthread consumer_thread(
//         []()
//         {
//             auto ret = run_consumer();
//             if (!ret)
//             {
//                 std::cerr << ret.error() << std::endl;
//             }
//         }
//     );

//     producer_thread.join();
//     consumer_thread.join();

//     std::cout << "Done\n";

//     return 0;
// }