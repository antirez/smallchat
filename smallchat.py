import select
import socket
import sys

WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick."
PREFIX = b"/nick "


class Channel:
    def __init__(self, conn, protocol_cls, notify_receive, notify_close):
        self.conn = conn
        self.notify_receive = notify_receive
        self.notify_close = notify_close
        self.fd = conn.fileno()
        self.protocol = protocol_cls(self._received)
        self.out_buffer = bytearray()

    def raw_receive(self):
        data = self.conn.recv(1024)
        if not data:
            self.notify_close(self)
            self.conn.close()
        else:
            self.protocol.decode(data)

    def raw_send(self):
        if self.out_buffer:
            sent = self.conn.send(self.out_buffer)
            self.out_buffer = self.out_buffer[sent:]

    def send(self, msg):
        self.out_buffer += self.protocol.encode(msg)


class Channels:
    def __init__(self, inputs, outputs):
        self.inputs = inputs
        self.outputs = outputs
        self.clients = {}

    def add(self, client):
        self.clients[client.fd] = client
        self.inputs.append(client.conn)
        self.outputs.append(client.conn)

    def delete(self, client):
        self.clients.pop(client.fd)
        self.inputs.remove(client.conn)
        self.outputs.remove(client.conn)

    def get(self, conn):
        return self.clients[conn.fileno()]


class Client(Channel):
    def __init__(self, conn, protocol_cls, publish, notify_close):
        super().__init__(conn, protocol_cls, self._received, notify_close)
        self.publish = publish
        self.nick = f"user:{conn.fileno()}"

    def _received(self, msg):
        if msg.startswith(PREFIX):
            self.nick = msg[len(PREFIX):].decode()
        else:
            self.publish(self, msg)


class Clients(Channels):
    def add(self, client):
        super().add(client)
        print(f"Connected client fd={client.fd}, nick={client.nick}")
        client.send(WELCOME)

    def delete(self, client):
        super().delete(client)
        print(f"Disconnected client fd={client.fd}, nick={client.nick}")

    def publish(self, sender, msg):
        response = sender.nick.encode() + b"> " + msg
        for client in self.clients.values():
            if client != sender:
                client.send(response)


class Protocol:
    def __init__(self, notify):
        self.notify = notify
        self.buff = bytearray()

    @staticmethod
    def encode(msg):
        return msg + b"\n"

    def decode(self, data):
        for car in data:
            if car == ord("\n"):
                self.notify(self.buff)
                self.buff.clear()
            else:
                self.buff.append(car)


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
                    client = Client(conn, Protocol, clients.publish, clients.delete)
                    clients.add(client)
                else:
                    client = clients.get(s)
                    assert client.conn == s
                    client.raw_receive()
            for s in outputready:
                if s.fileno() > 0:
                    client = clients.get(s)
                    assert client.conn == s
                    client.raw_send()



if __name__ == '__main__':
    main(*sys.argv[1:])

