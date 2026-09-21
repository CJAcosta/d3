#!/bin/sh
# Build if needed, then run the end-to-end boot tests under QEMU.
set -e

cd "$(dirname "$0")/.."

if ! command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "error: qemu-system-i386 is not installed; cannot run the boot tests" >&2
    exit 1
fi

make --no-print-directory all
exec python3 scripts/boot-test.py
