from queue import Queue, Empty
from subprocess import PIPE, Popen
from threading import Thread
from time import sleep
from unittest import TestCase


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


class TestIntegration(TestCase):
    CMD = ["nc", "localhost", "7711"]
    WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick.\n"

    def setUp(self):
        self.c_first = Process(self.CMD)
        self.c_second = Process(self.CMD)

    def tearDown(self):
        self.c_first.close()
        self.c_second.close()

    def test(self):
        l = self.c_first.read()
        self.assertEqual(l, self.WELCOME)
        l = self.c_second.read()
        self.assertEqual(l, self.WELCOME)
        self.c_first.write(b"/nick test-me\n")
        sleep(.1)  # AH!! I thiunk this sleep has something to do with Antirez next lesson ;-)
        self.c_first.write(b"Hi!\n")
        l_second = self.c_second.read()
        self.assertEqual(l_second, b"test-me> Hi!\n")
