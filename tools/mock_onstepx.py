#!/usr/bin/env python3
"""
mock_onstepx.py — fake OnStepX command server for contention testing.

Models the documented SWS/ESP32 behavior: four command channels
(9999/9998/9997/9996), each hosting a limited number of clients
(default 1, per the OnStep wiki). All channels share ONE command
processor with a serialization delay, like the real firmware's main loop.

Run on the laptop; point the real ESP32 handheld at the laptop's IP to do
full firmware validation with ZERO mount risk. Also used by
contention_test.py.

Usage:
  python mock_onstepx.py                        # defaults: 1 client/port
  python mock_onstepx.py --max-clients 2        # more permissive variant
  python mock_onstepx.py --busy-mode stall      # accept+ignore instead of refuse
  python mock_onstepx.py --proc-ms 10           # per-command processing delay

Every connection, refusal, command, and reply is logged with timestamps.
"""
import argparse
import asyncio
import math
import time

START = time.time()

def log(msg):
    print(f"[{time.time()-START:8.3f}] {msg}", flush=True)

class MountState:
    """Minimal OnStepX state machine: tracks, parks, jogs, fakes guiding."""
    def __init__(self):
        self.parked = False
        self.tracking = True
        self.ra0 = 0.712      # hours
        self.dec = 41.269     # deg
        self.rate = 5
        self.jog = set()      # active jog directions
        self.guide_pulse_until = 0.0

    def ra(self):
        # RA drifts slowly so poll data visibly changes
        return (self.ra0 + (time.time() - START) / 3600.0) % 24.0

    def lst(self):
        return (self.ra() + 1.5) % 24.0

    def gu(self):
        s = ""
        if not self.tracking: s += "n"
        s += "P" if self.parked else "p"
        if self.jog: s += "g"
        if time.time() < self.guide_pulse_until: s += "G"
        s += "N"          # no goto active
        s += "T"          # pier east
        s += str(self.rate)
        s += "0"          # error code 0
        return s

def sex(v, sign=False, degs=False):
    sg = "-" if v < 0 else ("+" if sign else "")
    v = abs(v)
    d = int(v); m = int((v - d) * 60); s = int(round(((v - d) * 60 - m) * 60))
    if s == 60: s = 0; m += 1
    sep = "*" if degs else ":"
    return f"{sg}{d:02d}{sep}{m:02d}:{s:02d}"

class Server:
    def __init__(self, args):
        self.args = args
        self.state = MountState()
        self.proc_lock = asyncio.Lock()     # ONE command processor, like firmware
        self.active = {}                     # port -> client count
        self.stats = {"cmds": 0, "refused": 0, "stalled": 0}

    def reply(self, cmd):
        st = self.state
        if cmd == ":GR#": return sex(st.ra()) + "#"
        if cmd == ":GD#": return sex(st.dec, sign=True, degs=True) + "#"
        if cmd == ":GA#": return sex(45.0 + 10 * math.sin(time.time()/30), sign=True, degs=True) + "#"
        if cmd == ":GZ#": return sex((120 + (time.time()-START)/10) % 360, degs=True) + "#"
        if cmd == ":GS#": return sex(st.lst()) + "#"
        if cmd == ":GU#": return st.gu() + "#"
        if cmd == ":D#":  return ("\x7f#" if st.jog else "#")
        if cmd == ":GE#": return "0#"
        if cmd == ":GVN#": return "10.25i#"
        if cmd == ":hP#": st.parked = True;  st.tracking = False; return "1"
        if cmd == ":hR#": st.parked = False; st.tracking = True;  return "1"
        if cmd == ":hC#": return "1"
        if cmd == ":Q#":  st.jog.clear(); return None
        if len(cmd) == 4 and cmd.startswith(":M") and cmd[2] in "nsew":
            st.jog.add(cmd[2]); return None
        if len(cmd) == 4 and cmd.startswith(":Q") and cmd[2] in "nsew":
            st.jog.discard(cmd[2]); return None
        if len(cmd) == 4 and cmd.startswith(":R") and cmd[2].isdigit():
            st.rate = int(cmd[2]); return None
        # Simulated guide pulse command family :Mg[nsew][ms]#
        if cmd.startswith(":Mg"):
            st.guide_pulse_until = time.time() + 0.5; return None
        return "0"  # unknown numeric-style failure

    async def handle(self, port, reader, writer):
        peer = writer.get_extra_info("peername")
        if self.active[port] >= self.args.max_clients:
            if self.args.busy_mode == "refuse":
                self.stats["refused"] += 1
                log(f"port {port}: REFUSED {peer} (channel full)")
                writer.close()
                return
            else:  # stall: accept, never service — models lwIP backlog accept
                self.stats["stalled"] += 1
                log(f"port {port}: STALLING {peer} (channel full, accepted but ignored)")
                await asyncio.sleep(30)
                writer.close()
                return
        self.active[port] += 1
        log(f"port {port}: connect {peer} (now {self.active[port]} on port)")
        buf = ""
        try:
            while True:
                data = await reader.read(64)
                if not data:
                    break
                buf += data.decode(errors="replace")
                while "#" in buf:
                    i = buf.index("#")
                    cmd = buf[:i+1]
                    buf = buf[i+1:]
                    async with self.proc_lock:      # firmware serialization
                        await asyncio.sleep(self.args.proc_ms / 1000.0)
                        r = self.reply(cmd)
                        self.stats["cmds"] += 1
                        if self.args.verbose:
                            log(f"port {port}: {cmd!r} -> {r!r}")
                    if r is not None:
                        writer.write(r.encode())
                        await writer.drain()
        except (ConnectionResetError, BrokenPipeError):
            pass
        finally:
            self.active[port] -= 1
            log(f"port {port}: disconnect {peer}")
            try:
                writer.close()
            except Exception:
                pass

    async def run(self):
        servers = []
        for port in self.args.ports:
            self.active[port] = 0
            s = await asyncio.start_server(
                lambda r, w, p=port: self.handle(p, r, w), self.args.bind, port)
            servers.append(s)
            log(f"listening on {self.args.bind}:{port} "
                f"(max {self.args.max_clients} client(s), busy={self.args.busy_mode})")
        try:
            while True:
                await asyncio.sleep(10)
                log(f"stats: {self.stats}  active={dict(self.active)}")
        finally:
            for s in servers:
                s.close()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", default="0.0.0.0")
    ap.add_argument("--ports", type=int, nargs="+", default=[9999, 9998, 9997, 9996])
    ap.add_argument("--max-clients", type=int, default=1,
                    help="clients per channel (OnStep wiki says 1)")
    ap.add_argument("--busy-mode", choices=["refuse", "stall"], default="refuse")
    ap.add_argument("--proc-ms", type=float, default=10.0,
                    help="per-command processing time (firmware serialization)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    asyncio.run(Server(args).run())

if __name__ == "__main__":
    main()
