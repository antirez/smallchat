from sys import executable
from time import sleep
from unittest import TestCase, skip

from process import Process
from smallchat import WELCOME

HOST = "localhost"
PORT = "7711"


class TestServer:
    def _wait_client_receive(self):
        pass

    def _wait_start_server(self):
        line = self.server.read()
        assert line.startswith(b"Server started")

    def setUp(self):
        self.server = Process(self.SERVER)
        self._wait_start_server()

    def tearDown(self):
        self.server.close()


class TestIntegration(TestServer):
    BIG_MESSAGE_BODY = b"Hi, it's " + b"me" * 10000 + b"."
    CLIENT = ["nc", HOST, PORT]
    CONSECUTIVE_MESSAGES_COUNT = 100
    CONTEMPORARY_CLIENTS_COUNT = 50

    def test_minimal(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        l = c_first.read()
        self.assertEqual(l, WELCOME)
        l = c_second.read()
        self.assertEqual(l, WELCOME)
        c_first.write(b"/nick test-me")
        self._wait_client_receive()
        c_first.write(b"Hi!")
        l_second = c_second.read()
        self.assertEqual(l_second, b"test-me> Hi!")
        c_first.close()
        c_second.close()

    def test_disconnected(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        c_third = Process(self.CLIENT)
        self.assertEqual(c_first.read(), WELCOME)
        self.assertEqual(c_second.read(), WELCOME)
        self.assertEqual(c_third.read(), WELCOME)
        c_third.close()
        c_first.write(b"/nick test-me")
        c_first.write(b"/nick test-me")
        self._wait_client_receive()
        c_first.write(b"Hi!")
        l_second = c_second.read()
        self.assertEqual(l_second, b"test-me> Hi!")
        c_first.close()
        c_second.close()

    def test_very_long_message(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        l = c_first.read()
        self.assertEqual(l, WELCOME)
        l = c_second.read()
        self.assertEqual(l, WELCOME)
        c_first.write(b"/nick test-me")
        self._wait_client_receive()
        msg = self.BIG_MESSAGE_BODY
        c_first.write(msg)
        l_second = c_second.read()
        self.assertEqual(l_second, b"test-me> " + msg)
        c_first.close()
        c_second.close()

    def test_many_consecutive_messages(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        self.assertEqual(c_first.read(), WELCOME)
        self.assertEqual(c_second.read(), WELCOME)
        c_first.write(b"/nick test-me")
        self._wait_client_receive()
        msg = self.BIG_MESSAGE_BODY
        for idx in range(self.CONSECUTIVE_MESSAGES_COUNT):
            c_first.write(msg)
        for idx in range(self.CONSECUTIVE_MESSAGES_COUNT):
            l_second = c_second.read()
            self.assertEqual(l_second, b"test-me> " + msg)
        c_first.close()
        c_second.close()

    def test_many_clients(self):
        clients = [Process(self.CLIENT) for idx in range(self.CONTEMPORARY_CLIENTS_COUNT)]
        for client in clients:
            self.assertEqual(client.read(), WELCOME)
        c_first, c_second, *c_others = clients
        c_first.write(b"/nick test-me-1")
        c_first.write(b"Hi, I'm the first!")
        c_second.write(b"/nick test-me-2")
        c_second.write(b"Hi, it's me, I'm the second!")
        self.assertEqual(c_first.read(), b"test-me-2> Hi, it's me, I'm the second!")
        self.assertEqual(c_second.read(), b"test-me-1> Hi, I'm the first!")
        for c_other in c_others:
            msgs = set([c_other.read(), c_other.read()])
            self.assertIn(b"test-me-1> Hi, I'm the first!", msgs)
            self.assertIn(b"test-me-2> Hi, it's me, I'm the second!", msgs)
        for client in clients:
            client.close()
 
 

class TestIntegrationPy(TestIntegration, TestCase):
    SERVER = [executable, "smallchat.py", "server", HOST, PORT]


class TestIntegrationClientPy(TestIntegration, TestCase):
    CLIENT = [executable, "smallchat.py", "client", HOST, PORT]
    CONTEMPORARY_CLIENTS_COUNT = 2
    SERVER = [executable, "smallchat.py", "server", HOST, PORT]

    @skip("TODO")
    def test_many_clients(self):
        super().test_many_clients()


class TestIntegrationC(TestIntegration, TestCase):
    SERVER = ["./smallchat-server"]

    def _wait_client_receive(self):
        sleep(.1)

    def _wait_start_server(self):
        sleep(.01)

    @skip("TODO")
    def test_very_long_message(self):
        super().test_very_long_message()

    @skip("TODO")
    def test_many_consecutive_messages(self):
        super().test_many_consecutive_messages()

    @skip("TODO")
    def test_many_clients(self):
        super().test_many_clients()
