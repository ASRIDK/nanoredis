"""A minimal Python client for the nanoredis wire protocol, used by the tests."""

import socket
import struct

TAG_NIL, TAG_ERR, TAG_STR, TAG_INT, TAG_DBL, TAG_ARR = range(6)


class ServerError(Exception):
    def __init__(self, code, message):
        super().__init__(f"({code}) {message}")
        self.code = code
        self.message = message


def encode_request(args):
    body = struct.pack("<I", len(args))
    for arg in args:
        data = arg.encode() if isinstance(arg, str) else arg
        body += struct.pack("<I", len(data)) + data
    return struct.pack("<I", len(body)) + body


def decode_value(data, pos=0):
    """Decodes one tagged value; returns (value, next_position)."""
    tag = data[pos]
    pos += 1
    if tag == TAG_NIL:
        return None, pos
    if tag == TAG_ERR:
        code, length = struct.unpack_from("<II", data, pos)
        pos += 8
        return ServerError(code, data[pos : pos + length].decode()), pos + length
    if tag == TAG_STR:
        (length,) = struct.unpack_from("<I", data, pos)
        pos += 4
        return data[pos : pos + length].decode(), pos + length
    if tag == TAG_INT:
        return struct.unpack_from("<q", data, pos)[0], pos + 8
    if tag == TAG_DBL:
        return struct.unpack_from("<d", data, pos)[0], pos + 8
    if tag == TAG_ARR:
        (count,) = struct.unpack_from("<I", data, pos)
        pos += 4
        items = []
        for _ in range(count):
            item, pos = decode_value(data, pos)
            items.append(item)
        return items, pos
    raise ValueError(f"unknown tag {tag}")


class Client:
    def __init__(self, port, host="127.0.0.1"):
        self.sock = socket.create_connection((host, port), timeout=10)

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _recv_exact(self, n):
        chunks = []
        while n > 0:
            chunk = self.sock.recv(n)
            if not chunk:
                raise ConnectionError("server closed the connection")
            chunks.append(chunk)
            n -= len(chunk)
        return b"".join(chunks)

    def send(self, *args):
        self.sock.sendall(encode_request(args))

    def recv(self):
        (length,) = struct.unpack("<I", self._recv_exact(4))
        value, end = decode_value(self._recv_exact(length))
        assert end == length, "trailing bytes in response"
        return value

    def call(self, *args):
        """Sends one command and returns its decoded response."""
        self.send(*args)
        return self.recv()
