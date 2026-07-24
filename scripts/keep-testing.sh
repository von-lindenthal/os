#!/bin/bash
# Additional regressions for keep-testing pass.
set -euo pipefail
cd "$(dirname "$0")/.."
make -s

run() {
  local out=$1; shift
  printf '%s\n' "$@" > /tmp/kt-in.txt
  set +e
  (sleep 0.5; cat /tmp/kt-in.txt) | timeout 8 qemu-system-i386 -kernel kernel.elf \
    -display none -serial stdio \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -nic none -no-reboot > "$out" 2>&1
  local rc=$?
  set -e
  if grep -qiE 'KERNEL PANIC|page fault|general protection' "$out"; then
    echo "FAIL panic in $out"
    tail -30 "$out"
    exit 1
  fi
  if [ "$rc" -ne 33 ] && [ "$rc" -ne 0 ]; then
    echo "FAIL qemu rc=$rc ($out)"
    tail -20 "$out"
    exit 1
  fi
}

fail=0
expect() { grep -qF "$2" "$1" || { echo "MISSING $1: $2"; fail=1; }; }
reject() { grep -qF "$2" "$1" && { echo "UNEXPECTED $1: $2"; fail=1; } || true; }

echo "=== calc INT_MIN/-1 ==="
run /tmp/kt-calc.txt 'calc -2147483648 / -1' 'calc 2000000000 + 2000000000' 'calc 1 + 2' 'halt'
expect /tmp/kt-calc.txt 'overflow'
expect /tmp/kt-calc.txt '3'
reject /tmp/kt-calc.txt 'KERNEL PANIC'

echo "=== nested run/repeat no smash ==="
run /tmp/kt-nest.txt \
  'write a repeat 1 run b' \
  'write b repeat 1 run c' \
  'write c repeat 1 run d' \
  'write d repeat 1 run e' \
  'write e echo deep' \
  'repeat 1 run a' \
  'help' \
  'about' \
  'halt'
expect /tmp/kt-nest.txt 'command nesting too deep'
expect /tmp/kt-nest.txt 'Games:'
expect /tmp/kt-nest.txt 'Themes stick'
expect /tmp/kt-nest.txt 'freestanding'

echo "=== long script line skipped ==="
python3 - <<'PY'
pad='A'*50
open('/tmp/kt-long-in.txt','w').write(
    f"touch s\nappend s #{pad}\nappend s {pad}echo HACKED\nrun s\nhalt\n")
PY
(sleep 0.5; cat /tmp/kt-long-in.txt) | timeout 5 qemu-system-i386 -kernel kernel.elf \
  -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -nic none -no-reboot > /tmp/kt-long.txt 2>&1 || true
expect /tmp/kt-long.txt 'script line too long (skipped)'
reject /tmp/kt-long.txt 'Unknown command'
# Must not execute the trailing fragment as a command
if grep -qE '^\+ .*HACKED' /tmp/kt-long.txt; then
  echo 'UNEXPECTED: HACKED executed as script line'
  fail=1
fi

echo "=== sort truncate warn / mv self / cp overwrite ==="
python3 - <<'PY'
cmds=['touch many']+[f'append many L{i:02d}\\n' for i in range(1,71)]+[
 'sort many','touch foo','mv foo foo','write s hello','write d world','cp s d','cat d','halt']
open('/tmp/kt-misc-in.txt','w').write('\n'.join(cmds)+'\n')
PY
(sleep 0.5; cat /tmp/kt-misc-in.txt) | timeout 8 qemu-system-i386 -kernel kernel.elf \
  -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -nic none -no-reboot > /tmp/kt-misc.txt 2>&1 || true
expect /tmp/kt-misc.txt '(sort truncated to 64 lines)'
if grep -A1 'mv foo foo' /tmp/kt-misc.txt | grep -q 'destination exists'; then
  echo 'UNEXPECTED: mv self failed'
  fail=1
fi
if ! grep -A1 'mv foo foo' /tmp/kt-misc.txt | grep -q 'ok'; then
  echo 'MISSING: mv self ok'
  fail=1
fi
expect /tmp/kt-misc.txt 'destination exists'
if ! grep -A1 'cat d' /tmp/kt-misc.txt | grep -q 'world'; then
  echo 'MISSING: cp refused overwrite'
  fail=1
fi

echo "=== append full reports truncated ==="
python3 - <<'PY'
chunk='X'*60
cmds=['touch full']+[f'append full {chunk}']*20+['append full MORE','halt']
open('/tmp/kt-full-in.txt','w').write('\n'.join(cmds)+'\n')
PY
(sleep 0.5; cat /tmp/kt-full-in.txt) | timeout 8 qemu-system-i386 -kernel kernel.elf \
  -display none -serial stdio -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -nic none -no-reboot > /tmp/kt-full.txt 2>&1 || true
expect /tmp/kt-full.txt 'truncated (file full)'

if [ "$fail" -ne 0 ]; then
  echo "KEEP-TESTING FAILED"
  exit 1
fi
echo "KEEP-TESTING OK"
