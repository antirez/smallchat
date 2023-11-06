from queue import Queue, Empty
from subprocess import PIPE, Popen
from sys import executable
from threading import Thread
from time import sleep
from unittest import TestCase, skip

from smallchat import WELCOME

HOST = "localhost"
PORT = "7711"


class Process:
    def __init__(self, cmd):
        self.p = Popen(cmd, stdin=PIPE, stdout=PIPE)
        self.q_in = Queue()
        self.t_in = Thread(target=self._populate)
        self.t_in.daemon = True
        self.t_in.start()
        self.q_out = Queue()
        self.t_out = Thread(target=self._consume)
        self.t_out.daemon = True
        self.t_out.start()

    def _consume(self):
        for line in iter(self.p.stdout.readline, b''):
            self.q_out.put(line)
        self.p.stdout.close()

    def _populate(self):
        while True:
            line = self.q_in.get()
            self.p.stdin.write(line)
            self.p.stdin.flush()

    def close(self):
        self.p.kill()
        self.p.wait()

    def read(self):
        return self.q_out.get()

    def write(self, line):
        self.q_in.put(line)


class TestIntegration:
    CLIENT = ["nc", HOST, PORT]

    def setUp(self):
        self.server = Popen(self.SERVER)
        sleep(.1)

    def tearDown(self):
        self.server.kill()
        self.server.wait()

    def test_minimal(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        l = c_first.read()
        self.assertEqual(l, WELCOME)
        l = c_second.read()
        self.assertEqual(l, WELCOME)
        c_first.write(b"/nick test-me\n")
        self.wait()
        c_first.write(b"Hi!\n")
        l_second = c_second.read()
        self.assertEqual(l_second, b"test-me> Hi!\n")
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
        c_first.write(b"/nick test-me\n")
        self.wait()
        c_first.write(b"Hi!\n")
        l_second = c_second.read()
        self.assertEqual(l_second, b"test-me> Hi!\n")
        c_first.close()
        c_second.close()

    def test_very_log_message(self):
        c_first = Process(self.CLIENT)
        c_second = Process(self.CLIENT)
        l = c_first.read()
        self.assertEqual(l, WELCOME)
        l = c_second.read()
        self.assertEqual(l, WELCOME)
        c_first.write(b"/nick test-me\n")
        self.wait()
        msg = b"Hi, it's " + b"me" * 1000 + b".\n"
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
        c_first.write(b"/nick test-me\n")
        self.wait()
        msg = b"Hi, it's " + b"me" * 1000 + b".\n"
        COUNT = 100
        for idx in range(COUNT):
            c_first.write(msg)
        for idx in range(COUNT):
            l_second = c_second.read()
            self.assertEqual(l_second, b"test-me> " + msg)
        c_first.close()
        c_second.close()

    def test_many_clients(self):
        clients = [Process(self.CLIENT) for idx in range(5)]
        for client in clients:
            self.assertEqual(client.read(), WELCOME)
        c_first, c_second, *c_others = clients
        c_first.write(b"/nick test-me-1\n")
        c_first.write(b"Hi, I'm the first!\n")
        c_second.write(b"/nick test-me-2\n")
        c_second.write(b"Hi, it's me, I'm the second!\n")
        self.assertEqual(c_first.read(), b"test-me-2> Hi, it's me, I'm the second!\n")
        self.assertEqual(c_second.read(), b"test-me-1> Hi, I'm the first!\n")
        for c_other in c_others:
            msgs = set([c_other.read(), c_other.read()])
            self.assertIn(b"test-me-1> Hi, I'm the first!\n", msgs)
            self.assertIn(b"test-me-2> Hi, it's me, I'm the second!\n", msgs)
        for client in clients:
            client.close()
 
 

class TestIntegrationPy(TestIntegration, TestCase):
    SERVER = [executable, "smallchat.py", HOST, PORT]

    def wait(self):
        pass


class TestIntegrationC(TestIntegration, TestCase):
    SERVER = ["./smallchat"]

    def wait(self):
        sleep(.1)

    @skip("TODO")
    def test_very_log_message(self):
        super().test_very_log_message()

    @skip("TODO")
    def test_many_consecutive_messages(self):
        super().test_many_consecutive_messages()

    @skip("TODO")
    def test_many_clients(self):
        super().test_many_clients()
