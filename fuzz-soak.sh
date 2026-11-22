#!/usr/bin/env bash
# 长测脚本：覆盖率引导(libFuzzer) + 自定义多策略 fuzz，长时间烘。
# 用法: sh fuzz-soak.sh [秒数=300]     例: sh fuzz-soak.sh 3600  (跑1小时)
set -uo pipefail
cd "$(dirname "$0")"
SECS=${1:-300}
INC="-Iinclude"
mkdir -p build/corpus
echo "== [1/2] libFuzzer 覆盖率引导 ${SECS}s =="
if command -v clang >/dev/null 2>&1; then
  clang -O1 -g -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer $INC \
    src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/fuzz_libfuzzer.c -o build/fuzz || exit 1
  ./build/fuzz build/corpus -max_total_time=$SECS -max_len=2048
else echo "(!) 无 clang，跳过 libFuzzer"; fi
echo "== [2/2] 自定义多策略 fuzz (ASan+UBSan) =="
gcc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $INC \
  src/jcb_read.c src/jcb_decode.c src/jcb_vm.c src/jcb_asm.c tests/fuzz_jcb.c -o build/fuzz-custom || exit 1
./build/fuzz-custom $((SECS*200000))
echo "长测完成（若打印 OK 且无 crash-* 文件即全清）"
