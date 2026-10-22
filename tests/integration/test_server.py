"""End-to-end tests that run the real server and talk to it over TCP."""

import socket
import subprocess
import time
import unittest
from pathlib import Path

from nanoredis_client import Client, ServerError, encode_request

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "build" / "nanoredis-server"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class ServerTestCase(unittest.TestCase):
    server_args = []

    def setUp(self):
        self.port = free_port()
        self.proc = subprocess.Popen(
            [str(SERVER), "-p", str(self.port), *self.server_args],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.time() + 5
        while True:
            try:
                self.client = Client(self.port)
                break
            except OSError:
                if time.time() > deadline:
                    raise
                time.sleep(0.02)

    def tearDown(self):
        self.client.close()
        self.proc.terminate()
        self.proc.wait(timeout=5)


class TestStrings(ServerTestCase):
    def test_get_set_del(self):
        c = self.client
        self.assertIsNone(c.call("get", "k"))
        self.assertIsNone(c.call("set", "k", "v"))
        self.assertEqual(c.call("get", "k"), "v")
        self.assertIsNone(c.call("set", "k", "v2"))
        self.assertEqual(c.call("get", "k"), "v2")
        self.assertEqual(c.call("del", "k"), 1)
        self.assertEqual(c.call("del", "k"), 0)
        self.assertIsNone(c.call("get", "k"))

    def test_keys_lists_every_key(self):
        for i in range(100):
            self.client.call("set", f"key{i}", str(i))
        self.assertEqual(sorted(self.client.call("keys")), sorted(f"key{i}" for i in range(100)))

    def test_unknown_command_is_an_error(self):
        err = self.client.call("nope")
        self.assertIsInstance(err, ServerError)
        self.assertEqual(err.code, 1)

    def test_large_values_round_trip(self):
        value = "x" * (1 << 20)
        self.client.call("set", "big", value)
        self.assertEqual(self.client.call("get", "big"), value)


class TestProtocol(ServerTestCase):
    def test_pipelined_requests_are_answered_in_order(self):
        c = self.client
        batch = b"".join(encode_request(["set", f"k{i}", f"v{i}"]) for i in range(1000))
        batch += b"".join(encode_request(["get", f"k{i}"]) for i in range(1000))
        c.sock.sendall(batch)
        for _ in range(1000):
            self.assertIsNone(c.recv())
        for i in range(1000):
            self.assertEqual(c.recv(), f"v{i}")

    def test_many_concurrent_clients(self):
        clients = [Client(self.port) for _ in range(50)]
        try:
            for i, c in enumerate(clients):
                c.send("set", f"client{i}", str(i))
            for c in clients:
                self.assertIsNone(c.recv())
            for i, c in enumerate(clients):
                self.assertEqual(c.call("get", f"client{i}"), str(i))
        finally:
            for c in clients:
                c.close()

    def test_malformed_request_closes_connection(self):
        c = self.client
        c.sock.sendall(b"\x04\x00\x00\x00\xff\xff\xff\xff")  # nstr far too large
        self.assertEqual(c.sock.recv(1), b"")
        # The server itself must still be healthy.
        with Client(self.port) as other:
            self.assertIsNone(other.call("get", "x"))


class TestSortedSets(ServerTestCase):
    def test_zadd_zscore_zrem(self):
        c = self.client
        self.assertEqual(c.call("zadd", "z", "1.5", "a"), 1)
        self.assertEqual(c.call("zadd", "z", "2.5", "a"), 0)
        self.assertEqual(c.call("zscore", "z", "a"), 2.5)
        self.assertIsNone(c.call("zscore", "z", "missing"))
        self.assertEqual(c.call("zrem", "z", "a"), 1)
        self.assertEqual(c.call("zrem", "z", "a"), 0)

    def test_zquery_returns_ordered_pages(self):
        c = self.client
        prices = {"bid1": 101.5, "bid2": 100.25, "bid3": 102.0, "bid4": 100.25}
        for name, price in prices.items():
            c.call("zadd", "book", str(price), name)

        everything = c.call("zquery", "book", "-inf", "", "0", "10")
        pairs = list(zip(everything[::2], everything[1::2]))
        self.assertEqual(
            pairs, [("bid2", 100.25), ("bid4", 100.25), ("bid1", 101.5), ("bid3", 102.0)]
        )
        self.assertEqual(c.call("zquery", "book", "101", "", "0", "10"), ["bid1", 101.5, "bid3", 102.0])
        self.assertEqual(c.call("zquery", "book", "100", "", "1", "2"), ["bid4", 100.25, "bid1", 101.5])
        self.assertEqual(c.call("zquery", "book", "200", "", "0", "10"), [])

    def test_zrank_and_zcard(self):
        c = self.client
        for i in range(100):
            c.call("zadd", "z", str(100 - i), f"m{i}")
        self.assertEqual(c.call("zcard", "z"), 100)
        self.assertEqual(c.call("zrank", "z", "m99"), 0)
        self.assertEqual(c.call("zrank", "z", "m0"), 99)

    def test_type_errors(self):
        c = self.client
        c.call("set", "s", "v")
        self.assertEqual(c.call("zadd", "s", "1", "a").code, 3)
        c.call("zadd", "z", "1", "a")
        self.assertEqual(c.call("get", "z").code, 3)
        self.assertEqual(c.call("zadd", "z", "nan", "a").code, 4)
        self.assertEqual(c.call("zquery", "z", "1", "", "x", "1").code, 4)


class TestExpiration(ServerTestCase):
    def test_pttl_reports_missing_and_persistent_keys(self):
        c = self.client
        self.assertEqual(c.call("pttl", "missing"), -2)
        c.call("set", "k", "v")
        self.assertEqual(c.call("pttl", "k"), -1)
        self.assertEqual(c.call("pexpire", "missing", "100"), 0)

    def test_key_expires_after_ttl(self):
        c = self.client
        c.call("set", "k", "v")
        self.assertEqual(c.call("pexpire", "k", "200"), 1)
        ttl = c.call("pttl", "k")
        self.assertTrue(0 < ttl <= 200)
        time.sleep(0.4)
        self.assertIsNone(c.call("get", "k"))
        self.assertEqual(c.call("pttl", "k"), -2)

    def test_negative_ttl_makes_key_persistent(self):
        c = self.client
        c.call("set", "k", "v")
        c.call("pexpire", "k", "200")
        c.call("pexpire", "k", "-1")
        self.assertEqual(c.call("pttl", "k"), -1)
        time.sleep(0.3)
        self.assertEqual(c.call("get", "k"), "v")

    def test_many_keys_expire(self):
        c = self.client
        for i in range(3000):
            c.send("set", f"k{i}", "v")
            c.send("pexpire", f"k{i}", str(50 + i % 100))
        for _ in range(6000):
            c.recv()
        time.sleep(0.5)
        self.assertEqual(c.call("keys"), [])

    def test_deleting_a_key_with_ttl_is_safe(self):
        c = self.client
        c.call("zadd", "z", "1", "a")
        c.call("pexpire", "z", "100")
        self.assertEqual(c.call("del", "z"), 1)
        time.sleep(0.2)
        self.assertEqual(c.call("keys"), [])


class TestIdleTimeout(ServerTestCase):
    server_args = ["-t", "300"]

    def test_idle_connection_is_closed(self):
        time.sleep(0.6)
        self.client.sock.settimeout(2)
        self.assertEqual(self.client.sock.recv(1), b"")

    def test_active_connection_is_kept_alive(self):
        for _ in range(6):
            time.sleep(0.1)
            self.assertIsNone(self.client.call("get", "k"))


if __name__ == "__main__":
    unittest.main()
