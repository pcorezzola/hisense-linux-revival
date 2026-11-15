import socket
import sys
import threading

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.0.56"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 2323

sock = socket.create_connection((HOST, PORT), timeout=10)
sock.settimeout(None)


def reader():
    while True:
        try:
            data = sock.recv(4096)
        except Exception:
            break
        if not data:
            break
        sys.stdout.write(data.decode("latin1", "replace"))
        sys.stdout.flush()
    print("\n[shell closed]")


thread = threading.Thread(target=reader, daemon=False)
thread.start()
try:
    sock.sendall(b"export PATH=/bin:/sbin:/usr/bin:/usr/sbin\n")
    sock.sendall(b"PS1='tv# '\n")
    while True:
        line = sys.stdin.readline()
        if not line:
            break
        sock.sendall(line.encode("latin1", "replace"))
        if line.strip() in ("exit", "logout"):
            break
except KeyboardInterrupt:
    pass
finally:
    try:
        sock.shutdown(socket.SHUT_WR)
    except Exception:
        pass
    try:
        thread.join(3.0)
    except Exception:
        pass
    try:
        sock.close()
    except Exception:
        pass
