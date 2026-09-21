#!/usr/bin/env python3
"""Boot the image in QEMU, then dump the VGA text screen and CPU state.

Used for debugging the parts of the boot that happen before the kernel's
serial console is alive.
"""
import os
import re
import socket
import subprocess
import sys
import tempfile
import time

IMAGE = sys.argv[1] if len(sys.argv) > 1 else "build/nebula.img"
WAIT = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0


def monitor_cmd(sock, cmd):
    sock.sendall((cmd + "\n").encode())
    time.sleep(0.4)
    out = b""
    sock.settimeout(0.5)
    try:
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            out += chunk
    except socket.timeout:
        pass
    text = out.decode(errors="replace")
    # The monitor echoes each keystroke with ANSI cursor motion; drop it.
    text = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", text)
    return text


def main():
    tmp = tempfile.mkdtemp()
    sock_path = os.path.join(tmp, "mon.sock")
    vga_path = os.path.join(tmp, "vga.bin")

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(sock_path)
    server.listen(1)

    qemu = subprocess.Popen([
        "qemu-system-i386", "-fda", IMAGE, "-display", "none",
        "-serial", "file:" + os.path.join(tmp, "serial.txt"),
        "-no-reboot",
        "-monitor", "unix:%s" % sock_path,
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    conn, _ = server.accept()
    time.sleep(WAIT)

    print("=== CPU state ===")
    print(monitor_cmd(conn, "info registers"))

    resp = monitor_cmd(conn, 'pmemsave 0xb8000 4000 "%s"' % vga_path)
    if not os.path.exists(vga_path):
        print("pmemsave response: %r" % resp[-200:])
    for _ in range(10):
        if os.path.exists(vga_path):
            break
        time.sleep(0.2)

    print("=== VGA text screen ===")
    try:
        with open(vga_path, "rb") as f:
            data = f.read()
        for row in range(25):
            line = "".join(
                chr(data[(row * 80 + col) * 2]) if 32 <= data[(row * 80 + col) * 2] < 127 else " "
                for col in range(80)
            )
            if line.strip():
                print("%2d | %s" % (row, line.rstrip()))
    except (OSError, IndexError) as exc:
        print("could not read VGA memory: %s" % exc)

    monitor_cmd(conn, "quit")
    try:
        qemu.wait(timeout=5)
    except subprocess.TimeoutExpired:
        qemu.kill()

    serial = os.path.join(tmp, "serial.txt")
    if os.path.exists(serial) and os.path.getsize(serial):
        print("=== serial ===")
        with open(serial, errors="replace") as f:
            print(f.read())


if __name__ == "__main__":
    main()
