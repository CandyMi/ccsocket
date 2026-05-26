# ccicmp ABI Reference

> 本文档由 `ccicmp.h` / `ccicmp.c` 源码自动生成。
> 描述 `ccicmp` 库的全部公开 ABI，依赖 [ccsocket ABI](ccsocket.md)。

---

## 1. `ccicmp_t` — ICMP 上下文

```c
typedef struct ccicmp_t {
    ccsocket_t fd;   // 原始套接字 — 参见 [ccsocket_t](ccsocket.md#11-基本类型)
    uint16_t   id;   // ICMP 标识符
    uint16_t   no;   // ICMP 序列号（每包递增）
} ccicmp_t;
```

---

## 2. `ccicmp_init`

```c
bool ccicmp_init(struct ccicmp_t *ctx, ccsocket_family_t domain);
```

- **说明**：初始化 ICMP ping 上下文，内部调用 [`ccsocket2`](ccsocket.md#ccsocket2) 创建原始套接字。
- **参数**：
  - `ctx`：未初始化的上下文指针。
  - `domain`：[`CC_INET4` 或 `CC_INET6`](ccsocket.md#23-ccsocket_family_t--地址族)。
- **权限**：大多数系统需要 root / `CAP_NET_RAW`；Windows 下直接失败。
- **返回值**：`true` 成功，`false` 失败（套接字创建失败或权限不足）。

---

## 3. `ccicmp_close`

```c
void ccicmp_close(struct ccicmp_t *ctx);
```

- **说明**：关闭套接字并释放上下文。内部调用 [`ccsocket_close`](ccsocket.md#ccsocket_close)。
- **副作用**：`ctx->fd` 置为 `INVALID_SOCKET`，`id`/`no` 归零。

---

## 4. `ccicmp_echo`

```c
bool ccicmp_echo(struct ccicmp_t *ctx, const char *addr, const char *data, size_t len);
```

- **说明**：向目标地址发送 ICMP Echo Request。
- **内部流程**：
  1. 调用 [`ccsocket_connect`](ccsocket.md#ccsocket_connect) 连接原始套接字到目标。
  2. 构建 ICMP 包：8 字节头部 + 8 字节时间戳 + 用户数据。
  3. 计算校验和后通过 [`ccsocket_send`](ccsocket.md#ccsocket_send1) 发送。
- **参数**：
  - `addr`：目标 IPv4/IPv6 地址字符串。
  - `data`：可选载荷（可为 `NULL`）。
  - `len`：载荷字节数，超过 [`CCICMP_MAX_PAYLOAD`](#7-编译期配置宏) 时自动截断。
- **Payload 结构**：

  ```
  [ICMP 头 8B] [时间戳 8B] [用户数据 len B]
  ```

  时间戳用于 RTT 计算：Windows 使用 `_ftime`（毫秒），POSIX 使用 `gettimeofday`（微秒）。
- **校验和**：
  - **IPv4**：16-bit 1's complement，仅覆盖 ICMP 报文。
  - **IPv6**：16-bit 1's complement，覆盖 40 字节伪首部 + ICMPv6 报文。伪首部源地址通过 [`ccsocket_get_peername`](ccsocket.md#ccsocket_get_peername) 获取。
- **返回值**：`true` 发送成功，`false` 失败（参数无效、连接失败或发送失败）。

---

## 5. `ccicmp_reply`

```c
bool ccicmp_reply(struct ccicmp_t *ctx, char *data, size_t *len);
```

- **说明**：接收匹配的 ICMP Echo Reply。
- **内部流程**：
  1. 调用 [`ccsocket_recv`](ccsocket.md#ccsocket_recv) 非阻塞接收。
  2. 跳过 IP 头（IPv4 解析 IHL，IPv6 自动检测是否含 IP 头）。
  3. 校验 ICMP 类型、ID 和序列号。
  4. 拷贝载荷到用户缓冲。
- **参数**：
  - `data`：输出缓冲区（可为 `NULL`，此时仅检查是否有应答）。
  - `len`：传入缓冲区容量，传出实际载荷长度。
- **IPv6 行为**：详见 [平台行为差异](#8-平台行为差异)。
- **返回值**：`true` 收到匹配的应答，`false` 超时或数据不匹配。

---

## 6. 内部辅助函数（非 ABI，仅供参考）

| 函数 | 说明 |
|---|---|
| `ccicmp_fill_timestamp(uint8_t *ts)` | 填入 8 字节时间戳 |
| `icmp_checksum_calc(buf, len)` | 计算 16-bit 1's complement 校验和 |
| `ccicmp_skip_ip_header(buf, len, af)` | 解析并跳过 IP 头，返回 ICMP 起始偏移 |

---

## 7. 编译期配置宏

| 宏 | 默认值 | 说明 |
|---|---|---|
| `CCICMP_MAX_PAYLOAD` | 65500 | 最大载荷字节数（`#ifndef` 可覆盖） |
| `CCICMP_RECV_BUFSZ` | 65535 | 接收缓冲区大小（`#ifndef` 可覆盖） |
| `IPPROTO_ICMPV6` | 58 | ICMPv6 协议号（系统无此宏时自动 `#define`） |

---

## 8. 平台行为差异

| 平台 | IPv6 套接字协议 | 接收是否含 IP 头 | 时间戳 |
|---|---|---|---|
| Linux | `IPPROTO_ICMPV6` (58) | 含 IPv6 头（跳过 40B） | `gettimeofday`（µs） |
| macOS/BSD | `IPPROTO_ICMPV6` (58) | **无** IP 头（偏移 0） | `gettimeofday`（µs） |
| Windows | `IPPROTO_ICMPV6` (58) | — | `_ftime`（ms） |

> **注意**：macOS/BSD 上 raw ICMPv6 套接字 [`ccsocket_recv`](ccsocket.md#ccsocket_recv) 返回的数据**不包含 IPv6 头**，直接以 ICMPv6 头开始。Linux 则在前面多 40 字节的 IPv6 头。`ccicmp_reply` 内部自动检测并适配两种行为。