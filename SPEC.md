# Jinvia · 能力格式 v0 (Capability Format)

> 无垠星河，无处不落。写一次，哪台机器都能跑。

一个「能力」= 一个普通文件夹。任何人、任何语言，写好丢给 jinvia，
那台设备当场"长出"这个本事 —— 不需要装系统、不需要装运行时。

## 一个能力，就两样东西

```
my-capability/
  capability.conf    # 声明：一行一条，纯文本，谁都能手写（纯 C 几十行就能读）
  main.wasm          # 实现：C / Rust / Zig / AssemblyScript… 能编到就能进
```

## capability.conf —— 全部就这几行（冒号分隔，零依赖）

```
name: thermostat-guard
does: 每5秒读一次温度，超过30度就报警
needs: read:temp, call:alarm
entry: main.wasm
```

## 三条铁律（Jinvia 的全部宪法）

1. **默认拒绝** —— needs 里没声明的能力，运行时一个都不给。
2. **零共享**   —— 能力与核不共享任何内存，只通过声明过的接口说话。
3. **无处不落** —— 同一个能力包，MCU 到服务器，原样运行。

## 跑起来

```
jinvia run thermostat-guard
```

## 为什么是这个格式

- **纯文本、一行一条** —— 谁都能手写，不需要任何库，天生就是"能力界的 JSON"。
- **解析器自己用 C 写** —— 不引依赖，贯彻"零外力"。别人要拉个 TOML/YAML 库，我们几十行搞定。
- **格式即契约** —— 格式稳了，核才敢长；格式乱，核越大越死。
