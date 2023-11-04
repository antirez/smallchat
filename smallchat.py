import select
import socket
import threading

ADDRESS = ("localhost", 7711)
WELCOME = b"Welcome to Simple Chat! Use /nick <nick> to set your nick.\n"
PREFIX = b"/nick "



CONNS = []


def serve(conn):
    with conn:
        print(f"Connected by {conn}")
        conn.sendall(WELCOME)
        while True:
            data = conn.recv(1024)
            if not data:
                break
            if data.startswith(PREFIX):
                nick = data[len(PREFIX):-1]
            else:
                response = nick + b"> " + data
                print("response", response)
                for c in CONNS:
                    if c != conn:
                        c.sendall(response)


def main():
    clients = []
    with socket.socket() as sl:
        sl.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sl.bind(ADDRESS)
        sl.listen()
        inputs = [sl]
        outputs = []
        while True:
            print(f"accepting...")
            inputready, outputready, exceptready = select.select(inputs, outputs, [])
            for s in inputready:
                if s == sl:
                    conn, addr = sl.accept()
                    print(f"accepted {addr}")
                    CONNS.append(conn)
                    th = threading.Thread(target=serve, args=(conn, ))
                    th.start()
                    clients.append(th)


if __name__ == '__main__':
    main()

