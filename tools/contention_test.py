#!/usr/bin/env python3
"""
contention_test.py — measures what happens when the LAPTOP (persistent
connection, like the ASCOM driver + PHD2 pulse guiding) and the HANDHELD
(connect-per-command, exactly the OnStepClient.h pattern: connect, 20ms
settle, send, read to '#', close) hit OnStep simultaneously.

Works against mock_onstepx.py now, and against the REAL mount later —
just change --host. Against the real mount it only sends read-only
queries plus optional :Mg guide pulses if --pulse is given.

Experiments:
  A  laptop persistent on 9999  + handheld per-command on 9999
  B  laptop persistent on 9999  + handheld per-command on 9998
  C  handheld alone, sweep pacing intervals (0/50/150 ms)
  D  guide-latency: laptop sends timed pulse commands while handheld polls

Reports per-client: success rate, connect failures, timeouts,
latency min/median/p95/max.
"""
import argparse
import socket
import statistics
import threading
import time

POLL = [":GU#", ":GR#", ":GD#", ":GA#", ":GZ#", ":GS#", ":D#"]

class Stats:
    def __init__(self, name):
        self.name = name
        self.lat = []
        self.ok = 0
        self.conn_fail = 0
        self.timeout = 0
        self.lock = threading.Lock()

    def add(self, ms, kind):
        with self.lock:
            if kind == "ok":
                self.ok += 1
                self.lat.append(ms)
            elif kind == "conn":
                self.conn_fail += 1
            else:
                self.timeout += 1

    def report(self):
        with self.lock:
            n = self.ok + self.conn_fail + self.timeout
            line = (f"  {self.name:28s} total={n:4d} ok={self.ok:4d} "
                    f"connFail={self.conn_fail:3d} timeout={self.timeout:3d}")
            if self.lat:
                line += (f"  lat ms: min={min(self.lat):6.1f} "
                         f"med={statistics.median(self.lat):6.1f} "
                         f"p95={statistics.quantiles(self.lat, n=20)[18] if len(self.lat)>=20 else max(self.lat):6.1f} "
                         f"max={max(self.lat):6.1f}")
            print(line)


def handheld_query(host, port, cmd, timeout_s=2.5):
    """Exact port of OnStepClient::send(): fresh TCP, 20ms settle, read to '#'."""
    t0 = time.time()
    try:
        s = socket.create_connection((host, port), timeout=timeout_s)
    except OSError:
        return (time.time() - t0) * 1000, "conn", ""
    try:
        time.sleep(0.02)
        s.sendall(cmd.encode())
        s.settimeout(timeout_s)
        buf = b""
        while b"#" not in buf and b"1" not in buf and b"0" not in buf:
            chunk = s.recv(64)
            if not chunk:
                return (time.time() - t0) * 1000, "timeout", buf.decode(errors="replace")
            buf += chunk
        return (time.time() - t0) * 1000, "ok", buf.decode(errors="replace")
    except socket.timeout:
        return (time.time() - t0) * 1000, "timeout", ""
    finally:
        s.close()


def handheld_loop(host, port, stats, stop, interval_ms):
    i = 0
    while not stop.is_set():
        cmd = POLL[i % len(POLL)]
        i += 1
        ms, kind, _ = handheld_query(host, port, cmd)
        stats.add(ms, kind)
        stop.wait(interval_ms / 1000.0)


def laptop_loop(host, port, stats, stop, interval_ms=250, pulse=False):
    """Persistent connection, like ASCOM driver. Reconnects if dropped."""
    sock = None
    i = 0
    while not stop.is_set():
        if sock is None:
            try:
                sock = socket.create_connection((host, port), timeout=2.5)
                sock.settimeout(2.5)
            except OSError:
                stats.add(0, "conn")
                stop.wait(0.5)
                continue
        cmd = ":Mgn0100#" if (pulse and i % 4 == 0) else POLL[i % len(POLL)]
        expect_reply = not cmd.startswith(":Mg")
        i += 1
        t0 = time.time()
        try:
            sock.sendall(cmd.encode())
            if expect_reply:
                buf = b""
                while b"#" not in buf and b"1" not in buf and b"0" not in buf:
                    chunk = sock.recv(64)
                    if not chunk:
                        raise socket.timeout
                    buf += chunk
            stats.add((time.time() - t0) * 1000, "ok")
        except (socket.timeout, OSError):
            stats.add((time.time() - t0) * 1000, "timeout")
            try:
                sock.close()
            except OSError:
                pass
            sock = None
        stop.wait(interval_ms / 1000.0)
    if sock:
        sock.close()


def run(name, seconds, threads_spec):
    print(f"\n=== {name} ({seconds}s) ===")
    stop = threading.Event()
    stats, threads = [], []
    for fn, st, kw in threads_spec:
        stats.append(st)
        t = threading.Thread(target=fn, kwargs=dict(stats=st, stop=stop, **kw), daemon=True)
        threads.append(t)
    for t in threads:
        t.start()
    time.sleep(seconds)
    stop.set()
    for t in threads:
        t.join(timeout=4)
    for st in stats:
        st.report()
    return stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--seconds", type=int, default=12)
    ap.add_argument("--pulse", action="store_true",
                    help="experiment D sends :Mg guide pulses (mock only unless you mean it)")
    ap.add_argument("--laptop-port", type=int, default=9999)
    ap.add_argument("--handheld-port", type=int, default=9998)
    a = ap.parse_args()
    H = a.host

    run("A: laptop persistent @9999 + handheld per-cmd @9999 (SAME port)", a.seconds, [
        (laptop_loop,   Stats("laptop persistent :9999"), dict(host=H, port=9999)),
        (handheld_loop, Stats("handheld per-cmd  :9999"), dict(host=H, port=9999, interval_ms=150)),
    ])

    run(f"B: laptop persistent @{a.laptop_port} + handheld per-cmd @{a.handheld_port} (SEPARATE ports)", a.seconds, [
        (laptop_loop,   Stats(f"laptop persistent :{a.laptop_port}"), dict(host=H, port=a.laptop_port)),
        (handheld_loop, Stats(f"handheld per-cmd  :{a.handheld_port}"), dict(host=H, port=a.handheld_port, interval_ms=150)),
    ])

    print("\n=== C: handheld alone, pacing sweep ===")
    for iv in (0, 50, 150):
        run(f"C: handheld alone @{a.handheld_port}, interval={iv}ms", max(6, a.seconds // 2), [
            (handheld_loop, Stats(f"handheld interval={iv:3d}ms"), dict(host=H, port=a.handheld_port, interval_ms=iv)),
        ])

    run("D: laptop GUIDING (timed cmds incl pulses) + handheld polling other port", a.seconds, [
        (laptop_loop,   Stats("laptop guide traffic"), dict(host=H, port=a.laptop_port, interval_ms=100, pulse=a.pulse)),
        (handheld_loop, Stats("handheld per-cmd poll"), dict(host=H, port=a.handheld_port, interval_ms=150)),
    ])
    run("D-baseline: laptop guiding traffic ALONE (no handheld)", a.seconds, [
        (laptop_loop, Stats("laptop guide traffic"), dict(host=H, port=a.laptop_port, interval_ms=100, pulse=a.pulse)),
    ])


if __name__ == "__main__":
    main()
