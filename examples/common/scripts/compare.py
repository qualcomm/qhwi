#!/usr/bin/env python3
#
# compare.py
# QHWI
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
"""
compare.py — compare two qhwi example runs and print a side-by-side delta table.

Usage:
    python compare.py <on_log> <off_log>

Each log file should contain the output of a single example run.  The script
parses the [stats] end-of-run summary and any [pil] metric lines emitted by
the Example base class when --pil-metrics is enabled.

Exit code 1 if either log is missing the required [stats] line.
"""

import re
import sys
from pathlib import Path

_STATS_RE = re.compile(
    r"\[stats\]"
    r"\s+frames:\s*(?P<frames>\d+)"
    r"\s+duration:\s*(?P<duration>[0-9.]+)s"
    r"\s+avg FPS:\s*(?P<fps>[0-9.]+)"
    r"\s+avg frame:\s*(?P<frame_ms>[0-9.]+)ms"
)
_PIL_RE = re.compile(r"\[pil\]\s+(?P<name>.+?):\s*(?P<value>[0-9.eE+\-]+)")


def parse_log(path: Path) -> dict:
    text = path.read_text(errors="replace")

    m = _STATS_RE.search(text)
    if not m:
        print(f"error: no [stats] line found in {path}", file=sys.stderr)
        sys.exit(1)

    result = {
        "frames":    int(m.group("frames")),
        "duration":  float(m.group("duration")),
        "fps":       float(m.group("fps")),
        "frame_ms":  float(m.group("frame_ms")),
        "pil":       {},
    }

    for pm in _PIL_RE.finditer(text):
        result["pil"][pm.group("name").strip()] = float(pm.group("value"))

    return result


def _delta_str(a: float, b: float, higher_is_better: bool) -> str:
    if b == 0:
        return "  n/a"
    pct = (a - b) / abs(b) * 100.0
    arrow = ""
    if pct > 0.5:
        arrow = " ▲" if higher_is_better else " ▼"
    elif pct < -0.5:
        arrow = " ▼" if higher_is_better else " ▲"
    return f"{pct:+.1f}%{arrow}"


def print_table(on: dict, off: dict, on_label: str, off_label: str) -> None:
    col_w = 14

    def row(label, on_val, off_val, fmt, higher_is_better=True):
        on_s  = fmt.format(on_val)
        off_s = fmt.format(off_val)
        delta = _delta_str(on_val, off_val, higher_is_better)
        print(f"  {label:<28}  {on_s:>{col_w}}  {off_s:>{col_w}}  {delta}")

    header = f"  {'metric':<28}  {on_label:>{col_w}}  {off_label:>{col_w}}  {'delta':>8}"
    print(header)
    print("  " + "-" * (len(header) - 2))

    row("avg FPS",       on["fps"],      off["fps"],      "{:.1f}",  higher_is_better=True)
    row("avg frame (ms)", on["frame_ms"], off["frame_ms"], "{:.2f}", higher_is_better=False)
    row("frames",        on["frames"],   off["frames"],   "{:d}",    higher_is_better=True)
    row("duration (s)",  on["duration"], off["duration"], "{:.1f}",  higher_is_better=False)

    pil_keys = sorted(set(on["pil"]) | set(off["pil"]))
    if pil_keys:
        print()
        print("  PIL metrics:")
        for key in pil_keys:
            on_v  = on["pil"].get(key, float("nan"))
            off_v = off["pil"].get(key, float("nan"))
            on_s  = f"{on_v:.4g}"  if on_v == on_v  else "n/a"
            off_s = f"{off_v:.4g}" if off_v == off_v else "n/a"
            if off_v == off_v and off_v != 0:
                delta = _delta_str(on_v, off_v, higher_is_better=False)
            else:
                delta = "  n/a"
            print(f"  {key:<28}  {on_s:>{col_w}}  {off_s:>{col_w}}  {delta}")


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: compare.py <on_log> <off_log>", file=sys.stderr)
        sys.exit(1)

    on_path  = Path(sys.argv[1])
    off_path = Path(sys.argv[2])

    for p in (on_path, off_path):
        if not p.exists():
            print(f"error: file not found: {p}", file=sys.stderr)
            sys.exit(1)

    on_data  = parse_log(on_path)
    off_data = parse_log(off_path)

    on_label  = on_path.stem
    off_label = off_path.stem

    print(f"\n  Comparing  ON={on_path}  vs  OFF={off_path}\n")
    print_table(on_data, off_data, on_label, off_label)
    print()


if __name__ == "__main__":
    main()
