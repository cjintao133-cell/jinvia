#!/usr/bin/env bash
# 构建静态库 libjinvia.a 和动态库 libjinvia.so（纯 C，零第三方依赖）
set -uo pipefail
cd "$(dirname "$0")"
CFLAGS="${CFLAGS:--O2 -Wall -Wextra -Werror -std=c11}"
INC="-Iinclude"
SRC="src/cap.c src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c src/jcb_verify.c src/jcb_disasm.c src/jcb_cc.c src/jinvia.c"
mkdir -p build/lib
echo "== 静态库 build/libjinvia.a =="
for s in $SRC; do
  gcc $CFLAGS $INC -fPIC -c "$s" -o "build/lib/$(basename "$s" .c).o" || exit 1
done
ar rcs build/libjinvia.a build/lib/*.o && echo "  OK build/libjinvia.a"
echo "== 动态库 build/libjinvia.so =="
gcc $CFLAGS $INC -fPIC -shared $SRC -o build/libjinvia.so && echo "  OK build/libjinvia.so"
