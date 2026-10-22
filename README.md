# nanoredis

[![CI](https://github.com/ASRIDK/nanoredis/actions/workflows/ci.yml/badge.svg)](https://github.com/ASRIDK/nanoredis/actions/workflows/ci.yml)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![License](https://img.shields.io/badge/license-MIT-green)

**nanoredis** is a single-threaded, event-driven, in-memory key-value server written from scratch
in C++17, modelled on the architecture of Redis. Every core data structure — the hashtable, the
balanced tree, the timer heap, the linked list — is implemented by hand, with no dependencies
beyond the C++ standard library and POSIX sockets.

The project explores the engineering behind low-latency infrastructure: non-blocking I/O
multiplexing, request pipelining, intrusive data structures that avoid allocations on the hot
path, incremental work scheduling so no request stalls the loop, and measuring tail latency rather
than averages. These are the same concerns that drive the design of market-data caches, order-book
services and other latency-sensitive systems.

---

## Table of contents

- [Features](#features)
- [Quick start](#quick-start)
- [Commands](#commands)
- [Architecture](#architecture)
- [Wire protocol](#wire-protocol)
- [Performance](#performance)
- [Testing](#testing)
- [Project history](#project-history)
- [Limitations](#limitations)
- [Acknowledgements](#acknowledgements)
- [License](#license)

## Features

- **Event loop** built on `poll()` with non-blocking sockets, serving thousands of concurrent
  clients on a single thread.
- **Pipelining**: many requests can be sent before reading any response; all complete requests in
  a read are processed in one pass.
- **Intrusive hashtable with progressive rehashing**: resizing migrates a bounded number of keys
  per operation, so there is never a stop-the-world rehash.
- **Sorted sets** indexed by both a hashtable (by member) and an **order-statistic AVL tree**
  (by score), giving `O(1)` point lookups and `O(log n)` range, rank and offset queries.
- **Key expiration** via a binary min-heap with back-references for `O(log n)` updates and
  removals, with a per-tick cap on expirations to bound latency.
- **Idle connection eviction** through an intrusive linked list ordered by last activity; the
  event loop sleeps exactly until the next timer is due.
- **Asynchronous destruction**: large sorted sets are freed on a worker thread pool so a single
  `del` cannot spike latency for every other client.
- **Benchmark tool** reporting throughput and p50/p90/p99/p99.9 latency.
- **Clean under ASan, UBSan and TSan**, with unit tests for every data structure and an
  end-to-end integration suite.

## Quick start

Requirements: a C++17 compiler (GCC or Clang), `make`, and Python 3.8+ for the integration tests.

```bash
git clone https://github.com/ASRIDK/nanoredis.git
cd nanoredis
make                          # builds the server, the CLI and the benchmark into ./build
./build/nanoredis-server      # listens on port 1234 (-p <port>, -t <idle-timeout-ms>)
```

In another terminal:

```text
$ ./build/nanoredis-cli zadd book 101.50 order-17
(int) 1
$ ./build/nanoredis-cli zadd book 100.25 order-42
(int) 1
$ ./build/nanoredis-cli zquery book -inf "" 0 10
(arr) len=4
  (str) order-42
  (dbl) 100.25
  (str) order-17
  (dbl) 101.5
(arr) end
$ ./build/nanoredis-cli set session abc
(nil)
$ ./build/nanoredis-cli pexpire session 5000
(int) 1
$ ./build/nanoredis-cli pttl session
(int) 4998
```

## Commands

| Command                                   | Reply                                  |
|-------------------------------------------|----------------------------------------|
| `get key`                                 | string, or nil                         |
| `set key value`                           | nil                                    |
| `del key`                                 | int: 1 if removed, 0 otherwise         |
| `keys`                                    | array of all keys                      |
| `pexpire key ms`                          | int: 1 if the key exists (`ms < 0` clears the TTL) |
| `pttl key`                                | int: remaining ms, -1 no TTL, -2 no key |
| `zadd key score member`                   | int: 1 if added, 0 if the score was updated |
| `zrem key member`                         | int: 1 if removed, 0 otherwise         |
| `zscore key member`                       | double, or nil                         |
| `zrank key member`                        | int: zero-based rank, or nil           |
| `zcard key`                               | int: number of members                 |
| `zquery key score member offset limit`    | flat array of `member, score` pairs starting at the first member `>= (score, member)`, skipping `offset` members |

`zquery` is a general range primitive: `zquery book 100 "" 0 10` returns the ten lowest entries at
or above a price of 100, and paging is a matter of increasing `offset`.

## Architecture

```text
                     ┌───────────────────────────────────────────────┐
  clients ──TCP──▶   │ event loop (server.cpp)                        │
                     │  poll() ─▶ read ─▶ parse ─▶ execute ─▶ write   │
                     │     ▲                                          │
                     │     └── timeout = next timer (idle list, heap) │
                     └───────┬───────────────────────────┬────────────┘
                             │                           │
              ┌──────────────▼──────────┐   ┌────────────▼─────────────┐
              │ keyspace: HMap          │   │ timers                    │
              │  Entry ─ string         │   │  DList  (idle connections)│
              │        └ ZSet           │   │  heap   (key TTLs)        │
              │            ├ HMap  (by member)                          │
              │            └ AVL   (by score)  └──────────────────────────┘
              └──────────────┬──────────┘
                             │ large values
                     ┌───────▼────────┐
                     │ ThreadPool     │  asynchronous destruction
                     └────────────────┘
```

| File               | Responsibility                                                    |
|--------------------|-------------------------------------------------------------------|
| `server.cpp`       | Event loop, connection state machine, command dispatch, timers.  |
| `protocol.*`       | Request decoding and typed response serialization.               |
| `buffer.h`         | FIFO byte buffer with `O(1)` consumption from the front.         |
| `hashtable.*`      | Intrusive chained hashtable with progressive rehashing.          |
| `avl.*`            | Intrusive AVL tree augmented with subtree sizes.                 |
| `zset.*`           | Sorted set combining the hashtable and the AVL tree.             |
| `heap.*`           | Binary min-heap with owner back-references.                      |
| `list.h`           | Intrusive circular doubly linked list.                           |
| `thread_pool.*`    | Fixed-size worker pool with a FIFO task queue.                   |
| `client.cpp`       | `nanoredis-cli`, a command-line client.                          |
| `bench.cpp`        | `nanoredis-bench`, a multi-threaded latency benchmark.           |

**Intrusive data structures.** Nodes embed their links directly (a member type inherits from
`HNode`, `AVLNode`, `DList` or `HeapSlot`), so a sorted-set member lives in a hashtable and a tree
at the same time with a single allocation, and owners are recovered with a type-safe
`static_cast` rather than pointer arithmetic.

## Wire protocol

All integers are little-endian.

```text
request   | body_len: u32 | nstr: u32 | len: u32 | bytes | len: u32 | bytes | ...
response  | body_len: u32 | tagged value |

tagged value
  NIL  0
  ERR  1 | code: u32 | len: u32 | message |
  STR  2 | len: u32 | bytes |
  INT  3 | i64 |
  DBL  4 | f64 |
  ARR  5 | n: u32 | n tagged values |
```

Error codes: `1` unknown command, `2` response too big, `3` wrong type, `4` bad argument.
A reference client in Python lives in [`tests/integration/nanoredis_client.py`](tests/integration/nanoredis_client.py).

## Performance

`make bench` starts a server and runs `nanoredis-bench` with 4 client threads, each issuing
requests back to back over loopback (one request in flight per connection, `TCP_NODELAY`):

| Test   | Throughput    | p50     | p90     | p99     | p99.9    |
|--------|---------------|---------|---------|---------|----------|
| `set`  | ~145k req/s   | 21 µs   | 40 µs   | 81 µs   | 190 µs   |
| `get`  | ~165k req/s   | 20 µs   | 33 µs   | 58 µs   | 105 µs   |
| `zadd` | ~174k req/s   | 20 µs   | 29 µs   | 51 µs   | 91 µs    |

Measured on an Apple M5 laptop; numbers are indicative and dominated by loopback round trips.
Pipelining (exercised in the integration tests) raises throughput substantially.

```bash
./build/nanoredis-bench -p 1234 -t zadd -n 500000 -c 8
```

## Testing

```bash
make test       # unit tests (data structures) + integration tests (real server over TCP)
make sanitize   # rebuild with ASan + UBSan (abort on first error) and run everything
make tsan       # rebuild with ThreadSanitizer and run everything
```

- **Unit tests** (`tests/unit`) check structural invariants after randomized workloads: AVL
  balance, heights, subtree counts and parent links; heap ordering and back-references;
  hashtable behaviour mid-rehash; sorted-set ordering, ranks and range seeks; thread-pool draining.
- **Integration tests** (`tests/integration`) start the server on a free port and cover every
  command, pipelining of 1,000 requests, 50 concurrent clients, 1 MiB values, malformed input,
  type errors, key expiry, idle eviction and asynchronous deletion.

CI runs the suite on Ubuntu (GCC, Clang) and macOS (Clang), plus ASan/UBSan and TSan jobs.

## Project history

The server was built incrementally; each commit is one stage:

1. Blocking TCP server and client
2. Length-prefixed message framing
3. Non-blocking `poll()` event loop with pipelining
4. `get` / `set` / `del`
5. Intrusive hashtable with progressive rehashing
6. Typed response serialization and `keys`
7. Order-statistic AVL tree
8. Sorted sets
9. Integration test suite
10. Idle-connection timers
11. Key expiration with a min-heap
12. Background thread pool for large deletions
13. Latency benchmark

## Limitations

nanoredis is an educational system, not a drop-in Redis replacement. It speaks its own binary
protocol rather than RESP, keeps data in memory only (no persistence or replication), and supports
the command subset listed above.

## Acknowledgements

The overall design follows the book
[Build Your Own Redis with C/C++](https://build-your-own.org/redis/) by James Smith, listed in
[build-your-own-x](https://github.com/codecrafters-io/build-your-own-x). This implementation
restructures the code into modules, replaces `offsetof`-based `container_of` with inheritance and
`static_cast`, adds `zrank`/`zcard`, a configurable server, a benchmark tool, and a unit plus
integration test suite that runs clean under sanitizers.

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for details.
