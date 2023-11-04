import select
import socket
import threading

ADDRESS = ("localhost", 7711)
WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick.\n"
PREFIX = b"/nick "
CONNS = {}


def serve(conn):
    with conn:
        fd = conn.fileno()
        # print(f"Connected by {conn} fd: {fd}")
        conn.sendall(WELCOME)
        CONNS[fd] = conn
        msg = b""
        while True:
            data = conn.recv(1024)
            if not data:
                CONNS.pop(fd)
                break
            msg += data
            if data[-1:] != b"\n":
                continue
            if msg.startswith(PREFIX):
                nick = msg[len(PREFIX):-1]
            else:
                response = nick + b"> " + msg
                for c in CONNS.values():
                    if c != conn:
                        c.sendall(response)
            msg = b""


def main():
    clients = []
    with socket.socket() as sl:
        sl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sl.bind(ADDRESS)
        sl.listen()
        inputs = [sl]
        outputs = []
        while True:
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                if s == sl:
                    conn, addr = sl.accept()
                    th = threading.Thread(target=serve, args=(conn, ))
                    th.start()
                    clients.append(th)


if __name__ == '__main__':
    main()

