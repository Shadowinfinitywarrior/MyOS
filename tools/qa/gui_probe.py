#!/usr/bin/env python3
"""Headless QEMU probe: boot myOS, inject input, capture screendumps.

Usage:
    gui_probe.py [--boot-timeout N] [--shot NAME]... [--keys SPEC] [--mouse SPEC]

Scripted actions are given in order, e.g.

    gui_probe.py --shot boot --keys "a,b,ret" --shot typed --mouse
    "move:400,300,click:rel" --shot clicked
"""
import argparse
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

QMP_PORT = 4444
QMP_PATH = f"/tmp/kilo/qmp-{QMP_PORT}.sock"


class Qmp:
    def __init__(self, path, timeout=30.0):
        deadline = time.time() + timeout
        self.sock = None
        while time.time() < deadline:
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.settimeout(10.0)
                s.connect(path)
                self.sock = s
                break
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.2)
        if self.sock is None:
            raise RuntimeError("could not connect to QMP socket")
        self.fp = self.sock.makefile("rwb")
        self._read()  # greeting
        self.cmd("qmp_capabilities")

    def _read(self):
        line = self.fp.readline()
        if not line:
            raise RuntimeError("QMP connection closed")
        return json.loads(line)

    def cmd(self, execute, **args):
        payload = {"execute": execute}
        if args:
            payload["arguments"] = args
        self.fp.write((json.dumps(payload) + "\n").encode())
        self.fp.flush()
        while True:
            msg = self._read()
            if "event" in msg:
                continue
            return msg

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


QCODE = {
    "ret": "ret", "enter": "ret", "esc": "esc", "tab": "tab",
    "space": "spc", "up": "up", "down": "down", "left": "left",
    "right": "right", "bs": "backspace", "del": "delete",
    "f1": "f1", "f2": "f2", "f3": "f3",
}
for _c in "abcdefghijklmnopqrstuvwxyz0123456789":
    QCODE[_c] = _c
for _i in range(1, 13):
    QCODE[f"f{_i}"] = f"f{_i}"


def type_keys(q, spec):
    """Type keys given as a comma-separated list of qcodes or names.

    The separator is a comma, not a colon: "h,e,l,p,ret" types help<Enter>,
    while "help:ENTER" is looked up as a single unknown key, falls through to
    the literal string, and is silently dropped by QEMU - which looks exactly
    like a dead keyboard. Colon-separated input reaches no key handler at all.
    """
    for raw in spec.split(","):
        raw = raw.strip()
        if not raw:
            continue
        qcode = QCODE.get(raw.lower(), raw.lower())
        q.cmd("send-key", keys=[{"type": "qcode", "data": qcode}])
        time.sleep(0.04)


_pointer = [512, 384]   # the guest driver starts the pointer at the centre
STEP = 64             # max relative delta per injected packet
NUDGE = 100           # closed-loop corrections stay small enough to land exactly


def _rel1(q, dx, dy):
    """Send exactly one relative motion event, no subdivision.

    A single modest delta reaches the guest intact; a rapid burst of many does
    not (QEMU's PS/2 re-chunking mangles them), so callers that need accuracy
    should issue several small events rather than one large one.
    """
    if dx == 0 and dy == 0:
        return
    q.cmd("input-send-event", events=[
        {"type": "rel", "data": {"axis": "x", "value": dx}},
        {"type": "rel", "data": {"axis": "y", "value": dy}},
    ])
    _pointer[0] = max(0, min(1023, _pointer[0] + dx))
    _pointer[1] = max(0, min(767, _pointer[1] + dy))


def _rel(q, dx, dy):
    """Emit one relative motion, split into bounded steps."""
    while dx or dy:
        sx = max(-STEP, min(STEP, dx))
        sy = max(-STEP, min(STEP, dy))
        q.cmd("input-send-event", events=[
            {"type": "rel", "data": {"axis": "x", "value": sx}},
            {"type": "rel", "data": {"axis": "y", "value": sy}},
        ])
        dx -= sx
        dy -= sy
        _pointer[0] = max(0, min(1023, _pointer[0] + sx))
        _pointer[1] = max(0, min(767, _pointer[1] + sy))
        time.sleep(0.04)


def _shoot(q, path, wait=40):
    if os.path.exists(path):
        os.unlink(path)
    q.cmd("screendump", filename=path)
    for _ in range(wait):
        if os.path.exists(path) and os.path.getsize(path) > 0:
            return True
        time.sleep(0.1)
    return False


def goto(q, tx, ty, outdir, budget=24):
    """Closed-loop pointer positioning.

    QEMU's PS/2 mouse re-chunks large relative jumps, so the guest does not
    land exactly where the harness asked. Rather than trust the arithmetic,
    read back where the guest actually drew the cursor and nudge until it
    agrees - which also verifies the cursor is being rendered at all.
    """
    path = os.path.join(outdir, "_goto.ppm")
    for attempt in range(budget):
        time.sleep(0.4)
        if not _shoot(q, path):
            return None
        hits = find_cursor(path)
        if not hits:
            lost = os.path.join(outdir, "_lost.ppm")
            os.replace(path, lost)
            print(f"  goto: cursor not found; frame saved to {lost}")
            return None
        cx, cy = min(hits, key=lambda p: abs(p[0] - tx) + abs(p[1] - ty))
        dx, dy = tx - cx, ty - cy
        if abs(dx) <= 3 and abs(dy) <= 3:
            print(f"  goto ({tx},{ty}) settled at ({cx},{cy})")
            return (cx, cy)
        print(f"  goto ({tx},{ty}): at ({cx},{cy}), nudging ({dx},{dy})")
        # One axis at a time: a combined x+y event does not survive QEMU's
        # PS/2 re-chunking intact, but single-axis moves land exactly.
        if dx:
            _rel1(q, max(-NUDGE, min(NUDGE, dx)), 0)
        if dy:
            _rel1(q, 0, max(-NUDGE, min(NUDGE, dy)))
    return None


def mouse(q, spec, outdir=None):
    """Absolute targets are converted to relative deltas.

    The guest has a PS/2 relative mouse, so QMP abs-axis events are dropped on
    the floor; only rel motion reaches it.

    spec (semicolon separated): goto:X,Y | abs:X,Y | rel:DX,DY | btn:B:down
    | btn:B:up | click:B | click:B:2 (double) | wheel:DIR | wheel:DIR:N
    """
    for raw in spec.split(";"):
        raw = raw.strip()
        if raw.startswith("goto:"):
            x, y = (int(v) for v in raw[5:].split(","))
            got = goto(q, x, y, outdir or "/tmp/kilo/shots")
            if got:
                _pointer[0], _pointer[1] = got
            else:
                x, y = max(0, min(1023, x)), max(0, min(767, y))
                _rel(q, x - _pointer[0], y - _pointer[1])
        elif raw.startswith("abs:") or raw.startswith("move:"):
            x, y = (int(v) for v in raw.split(":", 1)[1].split(","))
            x, y = max(0, min(1023, x)), max(0, min(767, y))
            _rel(q, x - _pointer[0], y - _pointer[1])
        elif raw.startswith("rel:"):
            dx, dy = (int(v) for v in raw[4:].split(","))
            _rel(q, dx, dy)
        elif raw.startswith("wheel:"):
            parts = raw[6:].split(":")
            btn = {"up": "wheel-up", "down": "wheel-down",
                   "left": "wheel-left", "right": "wheel-right"}[parts[0].lower()]
            times = int(parts[1]) if len(parts) > 1 else 1
            for _ in range(times):
                q.cmd("input-send-event", events=[
                    {"type": "btn", "data": {"down": True, "button": btn}}])
                time.sleep(0.04)
                q.cmd("input-send-event", events=[
                    {"type": "btn", "data": {"down": False, "button": btn}}])
                time.sleep(0.06)
        elif raw.startswith("btn:"):
            parts = raw[4:].split(":")
            btn = {"l": "left", "m": "middle", "r": "right"}[parts[0].lower()]
            state = parts[1] if len(parts) > 1 else "down"
            q.cmd("input-send-event", events=[
                {"type": "btn", "data": {"down": state == "down", "button": btn}}])
        elif raw.startswith("click:"):
            parts = raw[6:].split(":")
            btn = {"l": "left", "m": "middle", "r": "right"}[parts[0].lower()]
            times = int(parts[1]) if len(parts) > 1 else 1
            for _ in range(times):
                q.cmd("input-send-event", events=[
                    {"type": "btn", "data": {"down": True, "button": btn}}])
                time.sleep(0.05)
                q.cmd("input-send-event", events=[
                    {"type": "btn", "data": {"down": False, "button": btn}}])
                time.sleep(0.08)
        else:
            raise ValueError(f"bad mouse spec: {raw}")
        time.sleep(0.08)


ARROW = [
    "X...........",
    "XX..........",
    "X.X.........",
    "X..X........",
    "X...X.......",
    "X....X......",
    "X.....X.....",
    "X......X....",
    "X.......X...",
    "X........X..",
    "X.....XXXXX.",
    "X..X..X.....",
    "X.X.X..X....",
    "XX..X..X....",
    "X...X..X....",
    "....X..X....",
    "...X...X....",
    "..X....X....",
    ".X..........",
]
INK = [(c, r) for r, row in enumerate(ARROW) for c, ch in enumerate(row) if ch == "X"]
WHITE = (255, 255, 255)


def find_cursor(path):
    """Locate the software cursor by matching the exact arrow bitmap."""
    with open(path, "rb") as f:
        data = f.read()
    fields, idx = [], 2
    while len(fields) < 3:
        while data[idx:idx + 1].isspace():
            idx += 1
        start = idx
        while not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    w, h, _ = fields
    px = data[idx:idx + w * h * 3]

    hits = []
    # The guest clamps the pointer to the screen, so the arrow can hang off an
    # edge with part of it cut off. Accept a candidate whose visible ink pixels
    # are all white, and score it by how much of the arrow is on screen so a
    # fully-visible match always beats a clipped one.
    for y in range(-(len(ARROW) - 1), h):
        for x in range(-(len(ARROW[0]) - 1), w):
            onscreen = 0
            ok = True
            for c, r in INK:
                px_, py_ = x + c, y + r
                if not (0 <= px_ < w and 0 <= py_ < h):
                    continue
                onscreen += 1
                o = (py_ * w + px_) * 3
                if (px[o], px[o + 1], px[o + 2]) != WHITE:
                    ok = False
                    break
            if ok and onscreen >= 1:
                hits.append((onscreen, x, y))
    return [(x, y) for _, x, y in hits]


def analyze_ppm(path):
    """Return (unique_colors, nonblack_pct, top_colors)."""
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"P6"):
        raise ValueError("not a P6 PPM")
    # parse header: P6 <w> <h> <maxval> <binary>
    fields, idx = [], 2
    while len(fields) < 3:
        while data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b"#":
            while data[idx:idx + 1] not in (b"\n", b""):
                idx += 1
            continue
        start = idx
        while not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(int(data[start:idx]))
    idx += 1
    w, h, _ = fields
    px = data[idx:idx + w * h * 3]
    counts = {}
    nonblack = 0
    for i in range(0, len(px), 3):
        c = (px[i], px[i + 1], px[i + 2])
        counts[c] = counts.get(c, 0) + 1
        if c != (0, 0, 0):
            nonblack += 1
    total = w * h
    top = sorted(counts.items(), key=lambda kv: -kv[1])[:5]
    return {
        "size": f"{w}x{h}",
        "unique": len(counts),
        "nonblack_pct": round(100.0 * nonblack / total, 2),
        "top": [(f"#{r:02x}{g:02x}{b:02x}", round(100.0 * c / total, 1))
                for (r, g, b), c in top],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--boot-timeout", type=int, default=40)
    ap.add_argument("--shot", action="append", default=[])
    ap.add_argument("--keys", action="append", default=[])
    ap.add_argument("--mouse", action="append", default=[])
    ap.add_argument("--img", default="build/myos.img")
    ap.add_argument("--outdir", default="/tmp/kilo/shots")
    ap.add_argument("--serial", default="/tmp/kilo/gui_probe.log")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    if os.path.exists(QMP_PATH):
        os.unlink(QMP_PATH)

    ser = open(args.serial, "wb")
    qemu = subprocess.Popen([
        "qemu-system-x86_64",
        "-drive", f"file={args.img},format=raw,if=ide",
        "-m", "1G", "-smp", "1",
        "-serial", f"file:{args.serial}",
        "-display", "none",
        "-monitor", "none",
        "-qmp", f"unix:{QMP_PATH},server,nowait",
        "-no-reboot",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        q = Qmp(QMP_PATH)
        time.sleep(args.boot_timeout)
        results = {}
        for i, shot in enumerate(args.shot):
            time.sleep(0.8)
            path = os.path.join(args.outdir, f"{shot}.ppm")
            q.cmd("screendump", filename=path)
            for _ in range(50):
                if os.path.exists(path) and os.path.getsize(path) > 0:
                    break
                time.sleep(0.1)
            results[shot] = analyze_ppm(path)
            print(f"  captured {shot}")
            # Actions run AFTER their screenshot, so shot N is the state before
            # keys[N]/mouse[N] and shot N+1 shows the result.
            if i < len(args.keys):
                type_keys(q, args.keys[i])
            if i < len(args.mouse):
                mouse(q, args.mouse[i], args.outdir)
        for name, info in results.items():
            print(f"[{name}] {info['size']} unique={info['unique']} "
                  f"nonblack={info['nonblack_pct']}%")
            print("        top: " + ", ".join(f"{c} {p}%" for c, p in info["top"]))
        q.close()
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=5)
        except subprocess.TimeoutExpired:
            qemu.kill()
        ser.close()

    tail = open(args.serial, "rb").read()[-600:]
    print("--- serial tail ---")
    print(tail.decode("utf-8", "replace"))


if __name__ == "__main__":
    sys.exit(main())
