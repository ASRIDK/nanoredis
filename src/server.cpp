#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "buffer.h"
#include "hashtable.h"
#include "net.h"
#include "protocol.h"

using namespace nanoredis;

namespace {

// Response status codes.
enum : uint32_t {
    RES_OK = 0,
    RES_ERR = 1,  // error
    RES_NX = 2,   // key not found
};

struct Response {
    uint32_t status = RES_OK;
    std::string data;
};

// A key-value pair in the keyspace.
struct Entry : HNode {
    std::string key;
    std::string value;
};

// Lookup key: carries only the hash and the key bytes.
struct LookupKey : HNode {
    std::string key;
};

struct {
    HMap db;  // the top-level keyspace
} g_data;

bool entry_eq(const HNode *node, const HNode *key) {
    return static_cast<const Entry *>(node)->key == static_cast<const LookupKey *>(key)->key;
}

LookupKey make_key(const std::string &key) {
    LookupKey k;
    k.key = key;
    k.hcode = str_hash(reinterpret_cast<const uint8_t *>(key.data()), key.size());
    return k;
}

void do_get(const std::vector<std::string> &cmd, Response &out) {
    LookupKey key = make_key(cmd[1]);
    HNode *node = hm_lookup(&g_data.db, &key, entry_eq);
    if (node == nullptr) {
        out.status = RES_NX;
        return;
    }
    out.data = static_cast<Entry *>(node)->value;
}

void do_set(const std::vector<std::string> &cmd, Response &) {
    LookupKey key = make_key(cmd[1]);
    if (HNode *node = hm_lookup(&g_data.db, &key, entry_eq)) {
        static_cast<Entry *>(node)->value = cmd[2];
        return;
    }
    auto *entry = new Entry();
    entry->key = cmd[1];
    entry->value = cmd[2];
    entry->hcode = key.hcode;
    hm_insert(&g_data.db, entry);
}

void do_del(const std::vector<std::string> &cmd, Response &) {
    LookupKey key = make_key(cmd[1]);
    if (HNode *node = hm_delete(&g_data.db, &key, entry_eq)) {
        delete static_cast<Entry *>(node);
    }
}

void do_request(const std::vector<std::string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        do_del(cmd, out);
    } else {
        out.status = RES_ERR;
        out.data = "unknown command";
    }
}

// Response wire format: | len (4) | status (4) | data |
void make_response(const Response &resp, Buffer &out) {
    out.append_u32(static_cast<uint32_t>(4 + resp.data.size()));
    out.append_u32(resp.status);
    out.append(resp.data.data(), resp.data.size());
}

struct Conn {
    int fd = -1;
    // What the event loop should wait for on this socket.
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    Buffer incoming;  // bytes received but not yet parsed
    Buffer outgoing;  // responses not yet written
};

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        die("fcntl()");
    }
}

std::unique_ptr<Conn> handle_accept(int listen_fd) {
    sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(listen_fd, reinterpret_cast<sockaddr *>(&client_addr), &addrlen);
    if (connfd < 0) {
        return nullptr;
    }
    set_nonblocking(connfd);

    auto conn = std::make_unique<Conn>();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

// Parses and handles one complete request from the incoming buffer, if present.
// Returns false when more data is needed or the connection must be closed.
bool try_one_request(Conn &conn) {
    if (conn.incoming.size() < 4) {
        return false;
    }
    uint32_t len = 0;
    std::memcpy(&len, conn.incoming.data(), 4);
    if (len > kMaxMsg) {
        conn.want_close = true;
        return false;
    }
    if (conn.incoming.size() < 4 + static_cast<size_t>(len)) {
        return false;
    }

    const uint8_t *request = conn.incoming.data() + 4;
    std::vector<std::string> cmd;
    if (!parse_request(request, len, cmd)) {
        std::fprintf(stderr, "bad request\n");
        conn.want_close = true;
        return false;
    }

    Response resp;
    do_request(cmd, resp);
    make_response(resp, conn.outgoing);

    conn.incoming.consume(4 + len);
    return true;
}

void handle_write(Conn &conn) {
    ssize_t rv = write(conn.fd, conn.outgoing.data(), conn.outgoing.size());
    if (rv < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (rv < 0) {
        conn.want_close = true;
        return;
    }
    conn.outgoing.consume(static_cast<size_t>(rv));
    if (conn.outgoing.empty()) {
        conn.want_read = true;
        conn.want_write = false;
    }
}

void handle_read(Conn &conn) {
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn.fd, buf, sizeof(buf));
    if (rv < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (rv <= 0) {
        conn.want_close = true;  // error or EOF
        return;
    }
    conn.incoming.append(buf, static_cast<size_t>(rv));

    // Pipelining: a single read may contain several requests.
    while (try_one_request(conn)) {
    }

    if (!conn.outgoing.empty()) {
        conn.want_read = false;
        conn.want_write = true;
        // Optimistically write now; the socket is usually ready.
        handle_write(conn);
    }
}

int create_listener(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        die("bind()");
    }
    set_nonblocking(fd);
    if (listen(fd, SOMAXCONN) != 0) {
        die("listen()");
    }
    return fd;
}

}  // namespace

int main() {
    int listen_fd = create_listener(kDefaultPort);

    // Connections indexed by file descriptor.
    std::vector<std::unique_ptr<Conn>> fd2conn;
    std::vector<pollfd> poll_args;

    for (;;) {
        poll_args.clear();
        poll_args.push_back({listen_fd, POLLIN, 0});
        for (const auto &conn : fd2conn) {
            if (!conn) {
                continue;
            }
            short events = POLLERR;
            if (conn->want_read) {
                events |= POLLIN;
            }
            if (conn->want_write) {
                events |= POLLOUT;
            }
            poll_args.push_back({conn->fd, events, 0});
        }

        int rv = poll(poll_args.data(), static_cast<nfds_t>(poll_args.size()), -1);
        if (rv < 0 && errno == EINTR) {
            continue;
        }
        if (rv < 0) {
            die("poll()");
        }

        if (poll_args[0].revents) {
            if (auto conn = handle_accept(listen_fd)) {
                size_t fd = static_cast<size_t>(conn->fd);
                if (fd2conn.size() <= fd) {
                    fd2conn.resize(fd + 1);
                }
                fd2conn[fd] = std::move(conn);
            }
        }

        for (size_t i = 1; i < poll_args.size(); ++i) {
            short ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }
            Conn &conn = *fd2conn[static_cast<size_t>(poll_args[i].fd)];
            if (ready & POLLIN) {
                handle_read(conn);
            }
            if (ready & POLLOUT) {
                handle_write(conn);
            }
            if ((ready & POLLERR) || conn.want_close) {
                close(conn.fd);
                fd2conn[static_cast<size_t>(conn.fd)].reset();
            }
        }
    }
}
