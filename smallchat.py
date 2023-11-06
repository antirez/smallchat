import select
import socket
import sys
import threading

# TODO:
# ehm... so many race conditions at the moment


WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick.\n"
PREFIX = b"/nick "


class Pool:
    def __init__(self):
        self.conns = {}

    def add(self, conn):
        fd = conn.fileno()
        self.conns[fd] = conn

    def delete(self, conn):
        fd = conn.fileno()
        self.conns.pop(fd)

    def publish(self, sender, msg):
        response = sender.nick + b"> " + msg
        for conn in self.conns.values():
            if conn != sender:
                conn.sendall(response)


class Client:
    def __init__(self, pool, conn):
        self.pool = pool
        self.conn = conn
        self.nick = ""

    def _received(self, msg):
        if msg.startswith(PREFIX):
            self.nick = msg[len(PREFIX):-1]
        else:
            self.pool.publish(self, msg)

    def serve(self):
        with self.conn:
            self.conn.sendall(WELCOME)
            self.pool.add(self.conn)
            fd = self.conn.fileno()
            self.pool.conns[fd] = self.conn
            msg = bytearray()
            while True:
                data = self.conn.recv(1024)
                if not data:
                    self.pool.delete(self.conn)
                    break
                for car in data:
                    msg.append(car)
                    if car == ord("\n"):
                        self._received(msg)
                        msg.clear()


def main(host, port):
    address = (host, int(port))
    pool = Pool()
    clients = []
    with socket.socket() as sl:
        sl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sl.bind(address)
        sl.listen()
        inputs = [sl]
        outputs = []
        while True:
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                if s == sl:
                    conn, addr = sl.accept()
                    client = Client(pool, conn)
                    th = threading.Thread(target=client.serve)
                    th.start()
                    clients.append(th)


if __name__ == '__main__':
    main(*sys.argv[1:])

