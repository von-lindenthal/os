#!/bin/bash
# Edge-case stress test for os shell. Exit 0 if qemu exits with isa-debug-exit 0x10 (mapped to 33).
set -euo pipefail
cd "$(dirname "$0")/.."
make -s
dd if=/dev/zero of=/tmp/os-disk.img bs=1M count=8 status=none

# Build a longish payload (under INPUT_MAX=78)
LONG64=$(printf 'A%.0s' {1..64})
# Nearly-max file write via escapes (stay under FS_DATA_MAX)
FILL=$(python3 -c "print('x'*200)")

cat > /tmp/os-stress.txt <<EOF
about
help
# --- calc / peek ---
calc
calc 1 + 2
calc 10 / 0
calc 5 * 6
calc -3 + 7
bin 0
hex 0
base 0 2
base 255 2
# --- vars / aliases ---
set
set =bad
set ok=value
get ok
echo \$ok
unset ok
get ok
unset missing
alias
alias bad
alias a=echo hi
a
unalias a
unalias missing
alias loop=loop
loop
unalias loop
# --- fs edge cases ---
touch
touch bad/name
touch okfile
write okfile hello
append okfile \\nworld
cat okfile
wc okfile
head okfile 1
tail okfile 1
hexdump okfile
cp okfile ok2
mv ok2 ok3
cat ok3
rm ok3
find ok
write z.txt c\\nb\\na\\na
sort z.txt
uniq z.txt
rev z.txt
rev hello
copy okfile
clip
paste pasted.txt
cat pasted.txt
yank ${LONG64}
clip
# --- auth ---
passwd steal
login
login root wrong
login root os
whoami
passwd secret
logout
login root os
login root secret
whoami
logout
# --- misc ---
ps
debug
stopwatch reset
stopwatch start
sleep 1
stopwatch stop
stopwatch status
sleep 99999999
repeat 0 echo no
repeat 3 echo r
repeat 21 echo no
set x=1
env
vars
history
sysinfo
free
mem
cpu
ticks
uptime
date
motd
fortune
ascii
prime 1
prime 2
prime 9
prime 4294967291
fact 0
fact 12
fact 13
countdown 0
theme default
color
dmesg
lspci
disk
net
grep
grep hello okfile
diff
diff okfile pasted.txt
sum okfile
head readme.txt 0
tail readme.txt 0
# --- script run ---
write s.txt echo one\\necho two
run s.txt
# --- fill many files (capacity) ---
touch f01
touch f02
touch f03
touch f04
touch f05
touch f06
touch f07
touch f08
touch f09
touch f10
touch f11
touch f12
touch f13
touch f14
touch f15
touch f16
touch f17
touch f18
touch f19
touch f20
touch f21
ls
df
# --- empty / missing ---
cat missing
rm missing
cp missing x
mv missing x
head missing
tail missing
sort missing
uniq missing
hexdump missing
wc missing
copy missing
# comment line should be ignored
halt
EOF

echo "=== running stress test ==="
set +e
(sleep 0.6; cat /tmp/os-stress.txt) | qemu-system-i386 -kernel kernel.elf -display none \
  -drive file=/tmp/os-disk.img,format=raw,if=ide \
  -serial stdio \
  -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
  -nic none \
  -no-reboot > /tmp/os-stress-out.txt 2>&1
rc=$?
set -e
echo "qemu exit=$rc"
# isa-debug-exit status 0x10 => (0x10 << 1) | 1 = 33
if [ "$rc" -ne 33 ] && [ "$rc" -ne 0 ]; then
  echo "FAIL: unexpected qemu exit"
  tail -80 /tmp/os-stress-out.txt
  exit 1
fi

# Must have halted cleanly
grep -q 'Halting' /tmp/os-stress-out.txt || { echo "FAIL: no halt"; tail -40 /tmp/os-stress-out.txt; exit 1; }
# Must not panic
if grep -qiE 'panic|page fault|general protection|double fault' /tmp/os-stress-out.txt; then
  echo "FAIL: panic/fault detected"
  grep -iE 'panic|fault' /tmp/os-stress-out.txt
  exit 1
fi

# Spot-check expected outputs
fail=0
check() {
  if ! grep -qF "$1" /tmp/os-stress-out.txt; then
    echo "MISSING: $1"
    fail=1
  fi
}
check 'os 0.8'
check 'divide by zero'
check 'alias recursion limit'
check 'login required'
check 'bad password'
check 'welcome'
check 'filesystem full'
check 'file not found'
check 'n must be 1..20'
check 'max sleep is 300 seconds'
check 'PID  NAME'
check '=== debug ==='
check 'stopped 1.0s'
check '+ echo one'
check '+ echo two'
# comments must not error
if grep -q 'Unknown command: #' /tmp/os-stress-out.txt; then
  echo "FAIL: comment treated as command"
  fail=1
fi

if [ "$fail" -ne 0 ]; then
  echo "=== last 100 lines ==="
  tail -100 /tmp/os-stress-out.txt
  exit 1
fi

echo "STRESS OK"
# Show interesting snippets
grep -E 'filesystem full|divide by zero|alias recursion|bad password|welcome|Halting|PANIC' /tmp/os-stress-out.txt || true
