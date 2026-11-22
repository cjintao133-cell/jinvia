#!/usr/bin/env bash
# Jinvia 沙箱对抗演示 —— 让 7 个恶意能力去攻击宿主，看沙箱如何当场拦下。
# 用法:
#   bash examples/sandbox-showdown/showdown.sh          # 讲故事（彩色）
#   bash examples/sandbox-showdown/showdown.sh --check   # 只验结果，全部被拦则退出码 0
set -u
cd "$(dirname "$0")/../.."
JINVIA=${JINVIA_BIN:-./jinvia}
[ -x "$JINVIA" ] || make -s

QUIET=0; [ "${1:-}" = "--check" ] && QUIET=1

names=(a1-unauthorized a2-host-escape a3-divzero a4-sigfpe a5-oob-read a6-infinite a7-recursion)
intents=(
  "名分不足：申请一本不存在的『写磁盘』本事"
  "越界点名：偷偷调用第 9 号宿主能力（实际只注册了 4 个）"
  "除零炸弹：命令虚拟机算 100 / 0"
  "整数陷阱：INT64_MIN / -1 —— 朴素实现会 SIGFPE 崩掉整个进程"
  "越界读：从线性内存地址 -1 读取数据"
  "死循环：一个 JMP 自环，企图耗尽 CPU"
  "递归炸弹：无底自我调用，企图爆栈"
)
want=(3 3 4 4 4 4 4)

GRN=$'\033[1;32m'; RED=$'\033[1;31m'; DIM=$'\033[2m'; YEL=$'\033[1;33m'; RST=$'\033[0m'
if [ $QUIET -eq 0 ]; then
  echo "================================================================"
  echo "  Jinvia 沙箱对抗 —— 让 7 个恶意能力去攻击宿主"
  echo "================================================================"
  echo "引擎：纯 C、零依赖、默认拒绝的最小权限沙箱。"
  echo "规则：加载期被拒(exit 3) 或 运行期被拦(exit 4) = 宿主安然无恙。"
fi

fail=0; i=0; total=${#names[@]}
for n in "${names[@]}"; do
  d="examples/sandbox-showdown/$n"
  "$JINVIA" asm "$d/main.jcbasm" "$d/main.jcb" >/dev/null 2>&1
  out=$("$JINVIA" run "$d" 2>&1); rc=$?
  reason=$(printf '%s\n' "$out" | tail -n1)
  [ $QUIET -eq 0 ] && printf '\n[%d/%d] %s\n       %s\n' $((i+1)) $total "$n" "${intents[$i]}"
  if [ "$rc" = "${want[$i]}" ]; then
    tag=$([ "$rc" = "3" ] && echo "加载期拒绝" || echo "运行期拦截")
    [ $QUIET -eq 0 ] && printf '       -> %s   %s[ BLOCKED · %s ]%s\n' "$reason" "$GRN" "$tag" "$RST"
  else
    fail=$((fail+1))
    [ $QUIET -eq 0 ] && printf '       -> exit=%s   %s[ !! 未拦住 !! ]%s\n' "$rc" "$RED" "$RST"
  fi
  i=$((i+1))
done

if [ $QUIET -ne 0 ]; then
  echo "sandbox: $((total-fail))/$total blocked"
  exit $fail
fi
echo ""
echo "================================================================"
if [ $fail -eq 0 ]; then
  printf '结果：%s%d/%d 全部被拦，宿主零崩溃、零越权。%s\n' "$GRN" $total $total "$RST"
  echo "一句话：它想搞死宿主，宿主连眉头都不皱。"
else
  printf '%s有 %d 个恶意能力没被拦住！%s\n' "$RED" "$fail" "$RST"
fi
echo "================================================================"
exit $fail
