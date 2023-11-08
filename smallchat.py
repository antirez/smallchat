"""
TODO:
    avoid sendall (it could block)
"""

import select
import socket
import sys

WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick.\n"
PREFIX = b"/nick "


class Clients:
    def __init__(self, inputs, outputs):
        self.inputs = inputs
        self.outputs = outputs
        self.clients = {}

    def add(self, client):
        print(f"Connected client fd={client.fd}, nick={client.nick}")
        self.clients[client.fd] = client
        self.inputs.append(client.conn)
        self.outputs.append(client.conn)

    def delete(self, client):
        print(f"Disconnected client fd={client.fd}, nick={client.nick}")
        self.clients.pop(client.fd)
        self.inputs.remove(client.conn)
        self.outputs.remove(client.conn)

    def get(self, conn):
        return self.clients[conn.fileno()]

    def publish(self, sender, msg):
        response = sender.nick.encode() + b"> " + msg
        for client in self.clients.values():
            if client != sender:
                client.send(response)


class Protocol:
    def __init__(self, notify):
        self.notify = notify
        self.buff = bytearray()

    def put(self, data):
        for car in data:
            self.buff.append(car)
            if car == ord("\n"):
                self.notify(self.buff)
                self.buff.clear()


class Client:
    def __init__(self, conn, publish, notify_close):
        self.conn = conn
        self.publish = publish
        self.notify_close = notify_close
        self.fd = conn.fileno()
        self.nick = f"user:{self.fd}"
        self.protocol = Protocol(self._received)
        self.send(WELCOME)

    def _received(self, msg):
        if msg.startswith(PREFIX):
            self.nick = msg[len(PREFIX):-1].decode()
        else:
            self.publish(self, msg)

    def receive(self):
        data = self.conn.recv(1024)
        if not data:
            self.notify_close(self)
            self.conn.close()
            return False
        self.protocol.put(data)
        return True

    def send(self, response):
        self.conn.sendall(response)


def main(host, port):
    address = (host, int(port))
    with socket.socket() as sl:
        sl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sl.bind(address)
        sl.listen()
        inputs = [sl]
        outputs = []
        clients = Clients(inputs, outputs)
        while True:
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                if s == sl:
                    conn, addr = sl.accept()
                    client = Client(conn, clients.publish, clients.delete)
                    clients.add(client)
                else:
                    client = clients.get(s)
                    assert client.conn == s
                    client.receive()


if __name__ == '__main__':
    main(*sys.argv[1:])

