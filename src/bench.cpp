// nanoredis-bench: closed-loop latency and throughput benchmark.
//
// Each client thread holds one connection and issues requests back to back,
// timing every round trip. Results are aggregated into throughput and latency
// percentiles, which matter more than averages for latency-sensitive systems.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "net.h"
#include "protocol.h"

using namespace nanoredis;
using Clock = std::chrono::steady_clock;

namespace {

struct Options {
    uint16_t port = kDefaultPort;
    size_t requests = 100000;  // total across all clients
    size_t clients = 4;
    std::string test = "set";
};

void usage(const char *prog) {
    std::fprintf(stderr,
                 "usage: %s [-p port] [-n requests] [-c clients] [-t set|get|zadd]\n", prog);
    std::exit(1);
}

Options parse_options(int argc, char **argv) {
    Options opt;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            usage(argv[0]);
        }
        const char *flag = argv[i];
        const char *value = argv[i + 1];
        if (std::strcmp(flag, "-p") == 0) {
            if (!parse_port(value, opt.port)) {
                usage(argv[0]);
            }
        } else if (std::strcmp(flag, "-n") == 0) {
            opt.requests = std::strtoull(value, nullptr, 10);
        } else if (std::strcmp(flag, "-c") == 0) {
            opt.clients = std::strtoull(value, nullptr, 10);
        } else if (std::strcmp(flag, "-t") == 0) {
            opt.test = value;
        } else {
            usage(argv[0]);
        }
    }
    if (opt.requests == 0 || opt.clients == 0 ||
        (opt.test != "set" && opt.test != "get" && opt.test != "zadd")) {
        usage(argv[0]);
    }
    return opt;
}

int connect_to(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("connect()");
    }
    return fd;
}

std::vector<std::string> make_command(const std::string &test, size_t client, size_t i) {
    std::string key = "key:" + std::to_string(client) + ":" + std::to_string(i % 1000);
    if (test == "set") {
        return {"set", key, "value"};
    }
    if (test == "get") {
        return {"get", key};
    }
    return {"zadd", "bench:zset", std::to_string(i % 10007), key};
}

// Runs `count` requests on one connection and appends each latency in ns.
void run_client(const Options &opt, size_t client, size_t count, std::vector<uint64_t> &out) {
    int fd = connect_to(opt.port);
    std::vector<uint8_t> rbuf;
    out.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        std::vector<uint8_t> request = encode_request(make_command(opt.test, client, i));

        auto start = Clock::now();
        uint32_t len = 0;
        if (write_all(fd, request.data(), request.size()) != 0 ||
            read_full(fd, reinterpret_cast<uint8_t *>(&len), 4) != 0) {
            die("request failed");
        }
        rbuf.resize(len);
        if (read_full(fd, rbuf.data(), len) != 0) {
            die("request failed");
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start);
        out.push_back(static_cast<uint64_t>(elapsed.count()));
    }
    close(fd);
}

double percentile_us(const std::vector<uint64_t> &sorted, double p) {
    size_t idx = static_cast<size_t>(p / 100.0 * static_cast<double>(sorted.size() - 1));
    return static_cast<double>(sorted[idx]) / 1000.0;
}

}  // namespace

int main(int argc, char **argv) {
    Options opt = parse_options(argc, argv);

    std::vector<std::vector<uint64_t>> latencies(opt.clients);
    std::vector<std::thread> threads;
    size_t per_client = opt.requests / opt.clients;

    auto start = Clock::now();
    for (size_t c = 0; c < opt.clients; ++c) {
        threads.emplace_back(run_client, std::cref(opt), c, per_client, std::ref(latencies[c]));
    }
    for (std::thread &t : threads) {
        t.join();
    }
    double seconds = std::chrono::duration<double>(Clock::now() - start).count();

    std::vector<uint64_t> all;
    for (const auto &l : latencies) {
        all.insert(all.end(), l.begin(), l.end());
    }
    std::sort(all.begin(), all.end());

    std::printf("test:        %s\n", opt.test.c_str());
    std::printf("clients:     %zu\n", opt.clients);
    std::printf("requests:    %zu\n", all.size());
    std::printf("throughput:  %.0f req/s\n", static_cast<double>(all.size()) / seconds);
    std::printf("latency (us) p50=%.1f p90=%.1f p99=%.1f p99.9=%.1f max=%.1f\n",
                percentile_us(all, 50), percentile_us(all, 90), percentile_us(all, 99),
                percentile_us(all, 99.9), static_cast<double>(all.back()) / 1000.0);
    return 0;
}
