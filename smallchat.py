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

    def add(self, client):
        print(f"Connected client fd={client.fd}, nick={client.nick}")
        self.conns[client.fd] = client.conn

    def delete(self, client):
        print(f"Disconnected client fd={client.fd}, nick={client.nick}")
        self.conns.pop(client.fd)

    def publish(self, sender, msg):
        response = sender.nick.encode() + b"> " + msg
        for conn in self.conns.values():
            if conn != sender:
                conn.sendall(response)


class Client:
    def __init__(self, pool, conn):
        self.pool = pool
        self.conn = conn
        self.fd = conn.fileno()
        self.nick = f"user:{self.fd}"

    def _received(self, msg):
        if msg.startswith(PREFIX):
            self.nick = msg[len(PREFIX):-1].decode()
        else:
            self.pool.publish(self, msg)

    def serve(self):
        with self.conn:
            self.pool.add(self)
            self.conn.sendall(WELCOME)
            msg = bytearray()
            while True:
                data = self.conn.recv(1024)
                if not data:
                    break
                for car in data:
                    msg.append(car)
                    if car == ord("\n"):
                        self._received(msg)
                        msg.clear()
            self.pool.delete(self)


def main(host, port):
    address = (host, int(port))
    pool = Pool()
    threads = []
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
                    threads.append(th)


if __name__ == '__main__':
    main(*sys.argv[1:])

