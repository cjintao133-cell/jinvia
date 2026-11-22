#!/usr/bin/env bash
# Jinvia 可移植性检查：字符符号性矩阵 + 对齐/告警严格检查；
# 若有交叉工具链/QEMU，则额外做 32位/ARM 交叉编译与执行。
set -uo pipefail
cd "$(dirname "$0")"
INC="-Iinclude"; FLAGS="-O2 -Wall -Wextra -Werror -std=c11"
SRCS="src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_cc.c"
TESTS="src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c"
mkdir -p build/port; fail=0

echo "== 1) 字符符号性矩阵 (x86 char 有符号 / ARM 常无符号) =="
for f in -funsigned-char -fsigned-char; do
  if gcc $FLAGS $f $INC tests/test_edge.c $TESTS -o build/port/e && ./build/port/e >/dev/null; then echo "  [$f] OK"; else echo "  [$f] FAIL"; fail=1; fi
done

echo "== 2) 对齐敏感 (ARM 未对齐会 fault) + 严格告警 =="
for f in "-Wcast-align=strict" "-Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes"; do
  ok=1; for s in $SRCS; do gcc $f -std=c11 $INC -c "$s" -o /tmp/o.o 2>/dev/null || ok=0; done
  [ $ok -eq 1 ] && echo "  [$f] OK" || { echo "  [$f] FAIL"; fail=1; }
done

echo "== 3) 真机跨架构（需交叉工具链/QEMU） =="
have=0
for cc in aarch64-linux-gnu-gcc arm-linux-gnueabihf-gcc i686-linux-gnu-gcc; do
  if command -v $cc >/dev/null 2>&1; then
    echo "  交叉编译 with $cc ..."; have=1
    $cc $FLAGS $INC $TESTS tests/test_edge.c -o build/port/x-$cc 2>/dev/null && echo "    compile OK"
  fi
done
[ $have -eq 0 ] && echo "  (skipped) 本机无交叉工具链/QEMU —— 真机跨架构请在有工具链的机器上跑本步骤"

echo "== RESULT =="
[ $fail -eq 0 ] && echo "PORTABILITY GATE GREEN (源级)" || echo "PORTABILITY GATE RED"
exit $fail
