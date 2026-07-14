import sys, threading, time
sys.path.insert(0, '.')
from contention_test import Stats, laptop_loop, handheld_query

stop = threading.Event()
ls = Stats("laptop persistent :9999")
t = threading.Thread(target=laptop_loop, kwargs=dict(host="127.0.0.1", port=9999, stats=ls, stop=stop), daemon=True)
t.start()
time.sleep(0.5)
hs = Stats("handheld per-cmd  :9999")
tot0 = time.time()
for cmd in [":GU#", ":GR#", ":GD#", ":GA#"]:
    ms, kind, _ = handheld_query("127.0.0.1", 9999, cmd)
    print(f"  handheld {cmd}: {kind} in {ms:.0f} ms", flush=True)
    hs.add(ms, kind)
print(f"  handheld 4-command burst took {time.time()-tot0:.1f}s total")
stop.set(); t.join(timeout=3)
ls.report(); hs.report()
