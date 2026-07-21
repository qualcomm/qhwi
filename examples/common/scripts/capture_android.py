#!/usr/bin/env python3
#
# capture_android.py
# QHWI
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
"""
capture_android.py — launch a qhwi example on an Android device and capture its log.

Clears logcat, launches the activity with a cmdLine intent extra, streams logcat
until the [stats] end-of-run line appears plus a short drain window to catch
trailing [pil] lines, then writes the result to a file suitable for compare.py.

Usage:
    python capture_android.py <package> --cmdline "<args>" -o <output.log>

Example — capture two runs for comparison:
    python capture_android.py com.qualcomm.qhwi_example_qcom_image_proc \\
        --cmdline "--use-ext true  --pil-metrics true --num-frames 1800" \\
        -o run_on.log

    python capture_android.py com.qualcomm.qhwi_example_qcom_image_proc \\
        --cmdline "--use-ext false --pil-metrics true --num-frames 1800" \\
        -o run_off.log

    python compare.py run_on.log run_off.log
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path


_ACTIVITY    = "android.app.NativeActivity"
_LOG_TAG     = "Graphics"
_STATS_MARKER = "[stats]"
_DRAIN_SECS  = 2.0  # wait after [stats] to let trailing [pil] lines flush


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "package",
        help="Android package name, e.g. com.qualcomm.qhwi_example_qcom_image_proc",
    )
    parser.add_argument(
        "--cmdline",
        default="",
        help="Arguments forwarded to the example via the cmdLine intent extra",
    )
    parser.add_argument("-o", "--output", required=True, help="Output log file path")
    parser.add_argument(
        "--timeout",
        type=float,
        default=300.0,
        help="Max seconds to wait for the [stats] line (default: 300)",
    )
    parser.add_argument(
        "--serial", "-s",
        default=None,
        help="adb device serial number (passed as adb -s <serial>)",
    )
    args = parser.parse_args()

    adb_prefix = ["-s", args.serial] if args.serial else []

    def adb(*cmd, check=True, **kwargs):
        return subprocess.run(["adb"] + adb_prefix + list(cmd), check=check, **kwargs)

    component = f"{args.package}/{_ACTIVITY}"

    print(f"Clearing logcat buffer...")
    adb("logcat", "-c")

    print(f"Launching {component}")
    print(f"  cmdLine: {args.cmdline!r}")
    adb("shell", "am", "start", "-n", component, "--es", "cmdLine", args.cmdline)

    print(f"Streaming logcat (tag={_LOG_TAG}, timeout={args.timeout}s)...")
    proc = subprocess.Popen(
        ["adb"] + adb_prefix + ["logcat", "-s", f"{_LOG_TAG}:I"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )

    lines = []
    stats_seen_at = None
    start = time.monotonic()

    try:
        for raw in proc.stdout:
            line = raw.rstrip()
            lines.append(line)

            if _STATS_MARKER in line:
                stats_seen_at = time.monotonic()
                print(f"  {line}")

            if stats_seen_at is not None and (time.monotonic() - stats_seen_at) >= _DRAIN_SECS:
                break

            if (time.monotonic() - start) > args.timeout:
                print(
                    f"error: timed out after {args.timeout}s waiting for {_STATS_MARKER!r}",
                    file=sys.stderr,
                )
                proc.terminate()
                sys.exit(1)
    finally:
        proc.terminate()
        proc.wait()

    if stats_seen_at is None:
        print(
            f"error: logcat stream ended without a {_STATS_MARKER!r} line",
            file=sys.stderr,
        )
        sys.exit(1)

    out = Path(args.output)
    out.write_text("\n".join(lines) + "\n")
    pil_lines = [l for l in lines if "[pil]" in l and "monitoring" not in l]
    print(f"Wrote {len(lines)} lines to {out}  ({len(pil_lines)} [pil] metric lines)")


if __name__ == "__main__":
    main()
