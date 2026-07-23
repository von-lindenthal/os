#!/bin/bash
# Regression tests for bugs found in stress pass.
set -euo pipefail
cd "$(dirname "$0")/.."
make -s

run_cmds() {
  local out=$1
  shift
  printf '%s\n' "$@" > /tmp/os-bugfix-in.txt
  set +e
  (sleep 0.5; cat /tmp/os-bugfix-in.txt) | qemu-system-i386 -kernel kernel.elf \
    -display none -serial stdio \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -nic none -no-reboot > "$out" 2>&1
  local rc=$?
  set -e
  if [ "$rc" -ne 33 ] && [ "$rc" -ne 0 ]; then
    echo "FAIL qemu exit $rc"
    tail -40 "$out"
    exit 1
  fi
  if grep -qiE 'panic|page fault|general protection|double fault' "$out"; then
    echo "FAIL fault"
    exit 1
  fi
}

fail=0
expect() {
  if ! grep -qF "$2" "$1"; then
    echo "MISSING in $1: $2"
    fail=1
  fi
}
reject() {
  if grep -qF "$2" "$1"; then
    echo "UNEXPECTED in $1: $2"
    fail=1
  fi
}

echo "=== prime large (must not hang) ==="
timeout 4 bash -c '
  (sleep 0.5; printf "prime 4294967291\nprime 65537\nhalt\n") | \
  qemu-system-i386 -kernel kernel.elf -display none -serial stdio \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 -nic none -no-reboot
' > /tmp/bf-prime.txt 2>&1 || true
expect /tmp/bf-prime.txt "prime"
expect /tmp/bf-prime.txt "Halting"
reject /tmp/bf-prime.txt "terminating on signal"

echo "=== passwd requires login ==="
run_cmds /tmp/bf-auth.txt \
  'passwd steal' \
  'login root os' \
  'passwd secret' \
  'logout' \
  'login root os' \
  'login root secret' \
  'halt'
expect /tmp/bf-auth.txt "login required"
expect /tmp/bf-auth.txt "password updated"
expect /tmp/bf-auth.txt "bad password"
expect /tmp/bf-auth.txt "welcome"

echo "=== comments ignored ==="
run_cmds /tmp/bf-hash.txt \
  '# this is a comment' \
  'echo ok' \
  '!!' \
  'halt'
reject /tmp/bf-hash.txt "Unknown command: #"
expect /tmp/bf-hash.txt "ok"
# !! should replay echo ok, not the comment
expect /tmp/bf-hash.txt "echo ok"

echo "=== head 0 is empty ==="
run_cmds /tmp/bf-head.txt \
  'head readme.txt 0' \
  'tail readme.txt 0' \
  'halt'
# After "head readme.txt 0" prompt, next line should be the prompt again (no file body)
if grep -A1 'head readme.txt 0' /tmp/bf-head.txt | grep -q 'Welcome'; then
  echo "UNEXPECTED: head 0 printed file"
  fail=1
fi

echo "=== sleep cap ==="
run_cmds /tmp/bf-sleep.txt \
  'sleep 99999999' \
  'halt'
expect /tmp/bf-sleep.txt "max sleep is 300 seconds"
reject /tmp/bf-sleep.txt "sleeping..."

echo "=== unset missing ==="
run_cmds /tmp/bf-unset.txt \
  'unset nope' \
  'halt'
expect /tmp/bf-unset.txt "not found"

if [ "$fail" -ne 0 ]; then
  echo "BUGFIX TESTS FAILED"
  exit 1
fi
echo "BUGFIX TESTS OK"
