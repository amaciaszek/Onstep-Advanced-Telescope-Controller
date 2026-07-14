import sys, threading, time
sys.path.insert(0, '.')
from contention_test import Stats, laptop_loop, handheld_query

POLL = [":GU#", ":GR#", ":GD#", ":GA#", ":GZ#", ":GS#", ":D#"]
stop = threading.Event()
ls = Stats("laptop persistent :9999")
t = threading.Thread(target=laptop_loop, kwargs=dict(host="127.0.0.1", port=9999, stats=ls, stop=stop), daemon=True)
t.start(); time.sleep(0.5)

print("--- handheld full 7-cmd poll cycle on 9999 (laptop owns it), fixed fast-fail client ---")
t0 = time.time()
fails = 0
for cmd in POLL:
    ms, kind, _ = handheld_query("127.0.0.1", 9999, cmd)
    fails += (kind != "ok")
print(f"    cycle: {fails}/7 failed, total {time.time()-t0:.2f}s (vs ~17.5s with old full-timeout client)")

print('--- console: `port 9998` ---')
t0 = time.time()
vals = {}
for cmd in POLL:
    ms, kind, payload = handheld_query("127.0.0.1", 9998, cmd)
    vals[cmd] = (kind, payload.strip())
print(f"    cycle: all ok={all(k=='ok' for k,_ in vals.values())}, total {time.time()-t0:.2f}s")
for c,(k,p) in vals.items(): print(f"      {c} -> {p!r}")
stop.set(); t.join(timeout=3)
ls.report()
