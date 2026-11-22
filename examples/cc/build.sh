#!/usr/bin/env bash
# 把 examples/cc/*.c 逐个编译成各自目录下的 main.jcbasm 与 main.jcb。
# 用法：  bash examples/cc/build.sh            （在仓库根目录运行）
set -e
cd "$(dirname "$0")/../.."
J=./jinvia
for c in examples/cc/*.c; do
  name=$(basename "$c" .c)
  [ -d "examples/cc/$name" ] || continue
  "$J" cc "$c" "examples/cc/$name/main.jcbasm"
  "$J" asm "examples/cc/$name/main.jcbasm" "examples/cc/$name/main.jcb"
  echo "  -> examples/cc/$name"
done
echo "done."
