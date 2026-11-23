#!/usr/bin/env python3
"""Simulatore ELM327 WiFi (TCP) per test su PC senza auto.

Risponde come un adattatore ELM327 v1.5 collegato a una Mazda MX-5 NC (CAN 11/500):
PID 00/20/40, 0C, 0D, 05, 10, 11, 2F, 01, DTC mode 03/07, clear 04, ATRV.
Uso: python3 tools/fake_elm.py [porta]   (default 35000)
"""
import socket
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 35000


class Car:
    def __init__(self):
        self.t0 = time.time()
        self.dtcs = ["0133", "2004"]   # P0133 confermato, P2004 (IMRC) confermato
        self.pending = ["0301"]        # P0301 in attesa

    def speed(self):
        # accelera 0->120 km/h in 12 s, poi resta
        t = (time.time() - self.t0) % 20
        return max(0, min(120, int((t - 2) * 10))) if t > 2 else 0


def handle(conn, car):
    echo = True
    buf = b""
    searched = False
    conn.sendall(b"\r\r>")
    while True:
        data = conn.recv(256)
        if not data:
            return
        buf += data
        while b"\r" in buf:
            line, buf = buf.split(b"\r", 1)
            cmd = line.decode(errors="ignore").strip().upper().replace(" ", "")
            if not cmd:
                continue
            out = (cmd + "\r") if echo else ""
            if cmd == "ATZ":
                time.sleep(0.3)
                echo = True
                out = "ATZ\r\r\rELM327 v1.5\r"
            elif cmd == "ATE0":
                echo = False
                out += "OK\r"
            elif cmd == "ATDPN":
                out += "A6\r"
            elif cmd == "ATDP":
                out += "AUTO, ISO 15765-4 (CAN 11/500)\r"
            elif cmd == "ATRV":
                out += "14.1V\r"
            elif cmd.startswith("AT"):
                out += "OK\r"
            else:
                pre = "" if searched else "SEARCHING...\r"
                searched = True
                time.sleep(0.02)
                out += pre + obd(cmd, car) + "\r"
            conn.sendall((out + "\r>").encode())


def obd(cmd, car):
    v = car.speed()
    rpm = 900 + v * 45
    pids = {
        # due ECU rispondono (motore + cambio): l'app deve fare l'OR
        "0100": "4100BE3FB811\r410080180001",
        "0120": "412080020001",
        "0140": "414040000000",
        "010C": "410C%04X" % (rpm * 4),
        "010D": "410D%02X" % v,
        "0105": "4105%02X" % (88 + 40),
        "0110": "4110%04X" % int((3 + v * 0.25) * 100),
        "0111": "4111%02X" % (40 if v < 120 else 25),
        "012F": "412F%02X" % int(0.62 * 255),
        "0101": "4101%02X076500" % (0x80 | len(car.dtcs)),
    }
    if cmd in pids:
        return pids[cmd]
    if cmd == "03":
        if not car.dtcs:
            return "4300"
        return "43%02X%s" % (len(car.dtcs), "".join(car.dtcs))
    if cmd == "07":
        return "47%02X%s" % (len(car.pending), "".join(car.pending))
    if cmd == "04":
        car.dtcs, car.pending = [], []
        return "44"
    return "NO DATA"


def main():
    car = Car()
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", PORT))
    srv.listen(2)
    print("fake ELM327 su 127.0.0.1:%d" % PORT, flush=True)
    while True:
        c, _ = srv.accept()
        threading.Thread(target=handle, args=(c, car), daemon=True).start()


if __name__ == "__main__":
    main()
