import subprocess
import sys
import unittest


class Process:
    def __init__(self, args):
        self.proc = subprocess.Popen(
            args, stdout=subprocess.PIPE, stdin=subprocess.PIPE, bufsize=0)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        self.stop()
        self.proc.stdin.close()
        self.proc.stdout.close()

    def read(self):
        return self.proc.stdout.readline().strip()

    def stop(self):
        self.terminate()
        self.wait()

    def terminate(self):
        self.proc.terminate()

    def wait(self):
        self.proc.wait()

    def write(self, msg):
        # print(f"Process.write msg: {msg}", file=sys.stderr)
        self.proc.stdin.write(msg + b"\n")
        self.proc.stdin.flush()


class TestProcess(unittest.TestCase):
    def setUp(self):
        self.p = Process([sys.executable, __file__])

    def tearDown(self):
        self.p.close()

    def test_stdout(self):
        line = self.p.read()
        self.assertEqual(line, b"started")
        self.p.stop()
        line = self.p.read()
        self.assertFalse(line)

    def test_stdin(self):
        line = self.p.read()
        self.assertEqual(line, b"started")
        self.p.write(b"test-request")
        line = self.p.read()
        self.assertEqual(line, b"test-request")
        self.p.stop()
        line = self.p.read()
        self.assertFalse(line)

    def test_cycle(self):
        line = self.p.read()
        self.assertEqual(line, b"started")
        self.p.write(b"test-request-1")
        line = self.p.read()
        self.assertEqual(line, b"test-request-1")
        self.p.write(b"test-request-2")
        line = self.p.read()
        self.assertEqual(line, b"test-request-2")
        self.p.write(b"/exit")
        self.p.wait()
        line = self.p.read()
        self.assertFalse(line)


    def test_long(self):
        line = self.p.read()
        self.assertEqual(line, b"started")
        request = b"0123456789" * 10000
        self.p.write(request)
        line = self.p.read()
        self.assertEqual(line, request)
        self.p.stop()
        line = self.p.read()
        self.assertFalse(line)

 
class TestProcessContext(unittest.TestCase):
    def test_stdout(self):
        with Process([sys.executable, __file__]) as p:
            line = p.read()
            self.assertEqual(line, b"started")
            p.stop()
            line = p.read()
            self.assertFalse(line)


def test_main():
    sys.stdout.write("started\n")
    sys.stdout.flush()
    while True:
        # print("read", file=sys.stderr)
        request = sys.stdin.readline().strip()
        # print("request", request, file=sys.stderr)
        if request == '/exit':
            break
        sys.stdout.write(request + "\n")
        sys.stdout.flush()


if __name__ == '__main__':
    test_main(*sys.argv[1:])
