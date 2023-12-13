import select
import socket
import sys

WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick."
PREFIX = b"/nick "


class Client:
    def __init__(self, conn, protocol_cls, notify_receive, notify_close):
        self.conn = conn
        self.notify_receive = notify_receive
        self.notify_close = notify_close
        self.fd = conn.fileno()
        self.protocol = protocol_cls(self.notify_receive)
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
        # print(f"send msg: {msg}", file=sys.stderr)
        self.out_buffer += self.protocol.encode(msg)


class Clients:
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


class ChatClient(Client):
    def __init__(self, conn, protocol_cls, publish, notify_close):
        super().__init__(conn, protocol_cls, self._received, notify_close)
        self.publish = publish
        self.nick = f"user:{conn.fileno()}"

    def _received(self, msg):
        # print(f"received msg: {msg}", file=sys.stderr)
        if msg.startswith(PREFIX):
            self.nick = msg[len(PREFIX):].decode()
        else:
            self.publish(self, msg)


class ChatClients(Clients):
    def add(self, client):
        super().add(client)
        # print(f"Connected client fd={client.fd}, nick={client.nick}")
        client.send(WELCOME)

    def delete(self, client):
        super().delete(client)
        # print(f"Disconnected client fd={client.fd}, nick={client.nick}")

    def publish(self, sender, msg):
        response = sender.nick.encode() + b"> " + msg
        for client in self.clients.values():
            if client != sender:
                client.send(response)


class Protocol:
    END = b"\n"

    def __init__(self, notify, end=None):
        self.notify = notify
        self.end = end or self.END
        self.buff = bytearray()

    @classmethod
    def encode(cls, msg):
        assert not cls.END in msg
        return msg + cls.END

    def decode(self, data):
        for car in data:
            if car == ord(self.END):
                self.notify(self.buff)
                self.buff.clear()
            else:
                self.buff.append(car)


class Stream:
    def __init__(self, stdin, stdout):
        self.stdin = stdin
        self.stdout = stdout
        self.closed = False
        self.send = None

    def close(self):
        self.closed = True

    def raw_receive(self):
        msg = self.stdin.readline().rstrip()
        self.send(msg.encode())

    def receive(self, msg):
        self.stdout.write(msg.decode() + "\n")
        self.stdout.flush()

    def raw_send(self):
        pass # noop


def _main_client(address):
    stream = Stream(sys.stdin, sys.stdout)

    with socket.socket() as conn:
        conn.connect(address)
        client = Client(conn, Protocol, notify_receive=stream.receive, notify_close=stream.close)
        stream.protocol = Protocol(client.send, "\n")
        stream.send = client.send
        inputs = [conn, sys.stdin]
        outputs = [conn, sys.stdout]
        clients = {conn: client, sys.stdin: stream, sys.stdout: stream}
        while not stream.closed:
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                clients.get(s).raw_receive()
            for s in outputready:
                if s.fileno() <= 0:
                    # sockets already closed during reception/recv
                    continue
                clients.get(s).raw_send()


def _main_server(address):
    with socket.socket() as sl:
        sl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sl.bind(address)
        sl.listen()
        sys.stdout.write(f"Server started address={address}\n")
        sys.stdout.flush()
        inputs = [sl]
        outputs = []
        clients = ChatClients(inputs, outputs)
        while True:
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                if s == sl:
                    conn, addr = sl.accept()
                    client = ChatClient(conn, Protocol, clients.publish, clients.delete)
                    clients.add(client)
                else:
                    clients.get(s).raw_receive()
            for s in outputready:
                if s.fileno() <= 0:
                    # sockets already closed during reception/recv
                    continue
                clients.get(s).raw_send()


def main(role, host, port):
    fun = globals()["_main_" + role]
    address = (host, int(port))
    fun(address)


if __name__ == '__main__':
    main(*sys.argv[1:])

