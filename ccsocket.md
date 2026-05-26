# ccsocket ABI Reference

> 本文档由 `ccsocket.h` / `ccsocket.c` 源码自动生成。
> 描述 `ccsocket` 库的全部公开 ABI（Application Binary Interface），包括类型、常量、枚举、宏与导出函数。

---

## 1. 编译宏 / 类型定义

### 1.1 基本类型

| 宏 / typedef | 值 (Unix) | 值 (Windows) | 说明 |
|---|---|---|---|
| `ccsocket_t` | `int` | `intptr_t` | 套接字句柄类型 |
| `INVALID_SOCKET` | `(~0)` | 系统定义 | 无效套接字常量 |
| `cciovec_buf_t` | `void*` | `char*` | IO 向量缓冲区指针 |
| `cciovec_len_t` | `size_t` | `uint32_t` | IO 向量长度类型 |

### 1.2 长度常量

```c
#define MAX_ADDRLEN  255
#define MAX_ERRORLEN 255
```

### 1.3 `ccsocket_iovec_t` — IO 向量结构

```c
typedef struct ccsocket_iovec {
#if _WIN32               // Windows: len 在前
    cciovec_len_t  len;
    cciovec_buf_t  buf;
#else                    // Unix:    buf 在前
    cciovec_buf_t  buf;
    cciovec_len_t  len;
#endif
} ccsocket_iovec_t;
```

> ⚠️ 跨平台时**字段顺序不同**，不可将原始结构跨平台序列化。

### 1.4 IO 向量宏

```c
ccsocket_init_iov(iov, count)        // 清零 iov 数组
ccsocket_get_iov_len(iov, idx)       // 读取长度
ccsocket_set_iov_len(iov, idx, slen) // 设置长度
ccsocket_get_iov_buf(iov, idx)       // 读取缓冲区指针
ccsocket_set_iov_buf(iov, idx, sbuf) // 设置缓冲区指针
```

---

## 2. 枚举类型

### 2.1 `ccsocket_flags_t` — 套接字标志

```c
typedef enum {
    CC_NOFLAG   = 0,  // 无特殊标志
    CC_CLOEXEC  = 1,  // close-on-exec
    CC_NONBLOCK = 2,  // 非阻塞
} ccsocket_flags_t;
```

标志可按位组合：`CC_CLOEXEC | CC_NONBLOCK` 等价于 `3`。

### 2.2 `ccsocket_stcode_t` — send/recv 操作返回码

```c
typedef enum {
    CC_OPCODE_ERROR       = -1,  // 不可恢复错误，需关闭套接字
    CC_OPCODE_OK          =  0,  // 操作成功
    CC_OPCODE_WAIT        =  1,  // 需等待（非阻塞模式下缓冲区空/满）
    CC_OPCODE_WANT_REVENT =  2,  // 期望可读事件
    CC_OPCODE_WANT_WEVENT =  3,  // 期望可写事件
} ccsocket_stcode_t;
```

### 2.3 `ccsocket_family_t` — 地址族

```c
typedef enum {
    CC_FAMILY_INVALID = -1,  // 无效 / 未知
    CC_UNIX  = 0,            // AF_UNIX  Unix 域套接字
    CC_INET4 = 1,            // AF_INET   IPv4
    CC_INET6 = 2,            // AF_INET6  IPv6
} ccsocket_family_t;
```

### 2.4 `ccsocket_protocol_t` — 传输协议

```c
typedef enum {
    CC_PROTOCOL_INVALID = -1,  // 无效协议
    CC_TCP   = 1,              // SOCK_STREAM  + IPPROTO_TCP
    CC_UDP   = 2,              // SOCK_DGRAM   + IPPROTO_UDP
    CC_ICMP  = 3,              // SOCK_RAW     + IPPROTO_ICMP
} ccsocket_protocol_t;
```

### 2.5 `ccsocket_conn_state_t` — 连接状态

```c
typedef enum {
    CC_CONNERROR  = -1,  // 连接失败
    CC_CONNECTED  =  0,  // 已连接
    CC_CONNECTING =  1,  // 连接中（非阻塞模式）
} ccsocket_conn_state_t;
```

### 2.6 `ccsocket_sendf_state_t` — sendfile 状态

```c
typedef enum {
    CC_SENDERROR = -1,  // 不可恢复错误
    CC_SENDALL   =  0,  // 文件发送完毕
    CC_SENDWAIT  =  1,  // 发送缓冲区满，需等待
} ccsocket_sendf_state_t;
```

### 2.7 `ccaddrinfo_t` — 地址信息链表节点

```c
typedef struct ccaddrinfo {
    char              address[65];  // 点分/冒号分隔 IP 字符串
    ccsocket_family_t af;           // 地址族
    struct ccaddrinfo *next;        // 下一节点
} ccaddrinfo_t;
```

---

## 3. API 便捷宏

这些宏是对底层 ABI 函数的包装，用户在头文件可见。

```c
// 创建套接字
#define ccsocket(domain, protocol) \
    ccsocket2((domain), (protocol), CC_NOFLAG)

#define ccsocket1(domain, protocol, flags) \
    ccsocket2((domain), (protocol), (ccsocket_flags_t)(flags))

// 接受连接
#define ccsocket_accept(s, flags) \
    ccsocket_accept2((s), NULL, NULL, (ccsocket_flags_t)(flags))

#define ccsocket_accept1(s, paddr, pport, flags) \
    ccsocket_accept2((s), (paddr), (pport), (ccsocket_flags_t)(flags))

// socketpair
#define ccsocketpair(fds, flags) \
    ccsocketpair1((fds), (ccsocket_flags_t)(flags))

// 发送
#define ccsocket_send(s, buf, bsize, wsizep) \
    ccsocket_send1((s), (buf), (bsize), (wsizep), 0)

#define ccsocket_sendv(s, iov, iovcnt, wsizep) \
    ccsocket_sendv1((s), (iov), (iovcnt), (wsizep), 0)
```

---

## 4. 导出函数 (ABI)

### 4.1 创建与关闭

#### `ccsocket_close`
```c
int ccsocket_close(ccsocket_t s);
```
- **说明**：关闭套接字。
- **返回值**：0 成功，非 0 失败。

#### `ccsocket2`
```c
ccsocket_t ccsocket2(ccsocket_family_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags);
```
- **说明**：创建套接字（底层 ABI）。
- **参数**：
  - `domain`：`CC_UNIX` / `CC_INET4` / `CC_INET6`
  - `proto`：`CC_TCP` / `CC_UDP` / `CC_ICMP`
  - `flags`：`CC_NOFLAG` / `CC_CLOEXEC` / `CC_NONBLOCK`
- **返回值**：有效套接字句柄，失败返回 `INVALID_SOCKET`。

#### `ccsocketpair1`
```c
bool ccsocketpair1(ccsocket_t sv[2], ccsocket_flags_t flags);
```
- **说明**：创建一对已连接的流式套接字（类似 `socketpair`）。
- **参数**：
  - `sv`：输出，长度为 2 的套接字数组。
  - `flags`：套接字标志。
- **Windows 实现**：通过 TCP 本地回环模拟。
- **返回值**：`true` 成功，`false` 失败。

---

### 4.2 服务端

#### `ccsocket_listen`
```c
bool ccsocket_listen(ccsocket_t s, const char *addr, uint16_t port);
```
- **说明**：绑定地址并监听（独占模式）。
- **参数**：
  - `s`：由 `ccsocket2` 创建的套接字。
  - `addr`：IP 地址字符串（如 `"0.0.0.0"`），Unix 域为路径。
  - `port`：端口号，Unix 域传 0。
- **行为**：优先使用 `SO_EXCLUSIVEADDRUSE`（Windows）或 `SO_EXCLBIND`（Solaris），其他平台降级为 `SO_REUSEADDR`。
- **返回值**：`true` 成功。

#### `ccsocket_listen1`
```c
bool ccsocket_listen1(ccsocket_t s, const char *addr, uint16_t port);
```
- **说明**：绑定地址并监听（负载均衡模式，允许多进程/线程共享端口）。
- **平台支持**：Linux 3.9+, DragonFlyBSD 3.6+, FreeBSD 12+, Solaris 11.4+, AIX 7.2.5.0+。
- **错误处理**：失败时 `errno` / `WSAGetLastError()` 保留底层原始错误（如 `EADDRINUSE`），不会被覆盖为 `EINVAL`。
- **返回值**：`true` 成功。

#### `ccsocket_accept2`
```c
ccsocket_t ccsocket_accept2(ccsocket_t s, OPTIONAL char *addr, OPTIONAL uint16_t *port, ccsocket_flags_t flags);
```
- **说明**：从监听套接字接受一个客户端连接（底层 ABI）。
- **参数**：
  - `s`：监听套接字。
  - `addr`：可选，输出客户端 IP / Unix 路径，缓冲区 ≥ `MAX_ADDRLEN`。
  - `port`：可选，输出客户端端口。
  - `flags`：新套接字的标志。
- **非阻塞行为**：无连接且未出错时返回 `(ccsocket_t)0`（Unix）或 `0`。
- **返回值**：新客户端套接字，失败返回 `INVALID_SOCKET`，需等待返回 `0`。

---

### 4.3 客户端

#### `ccsocket_connect`
```c
bool ccsocket_connect(ccsocket_t s, const char *addr, uint16_t port);
```
- **说明**：连接到指定地址和端口。
- **参数**：
  - `s`：由 `ccsocket2` 创建的套接字。
  - `addr`：目标 IP / Unix 路径。
  - `port`：目标端口。
- **返回值**：`true` 成功。

#### `ccsocket_is_connected`
```c
ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s);
```
- **说明**：查询套接字连接状态。
- **返回值**：`CC_CONNECTED` / `CC_CONNECTING` / `CC_CONNERROR`。

---

### 4.4 数据收发

#### `ccsocket_send1`
```c
ccsocket_stcode_t ccsocket_send1(ccsocket_t s, const void *buf, size_t bsize, OPTIONAL int *wsize, int flags);
```
- **说明**：发送数据（底层 ABI）。
- **返回值**：
  - `CC_OPCODE_OK`    — 发送完成，`*wsize` 为写入字节数。
  - `CC_OPCODE_WAIT`  — 非阻塞模式下发送缓冲区满。
  - `CC_OPCODE_ERROR` — 不可恢复错误。

#### `ccsocket_sendv1`
```c
ccsocket_stcode_t ccsocket_sendv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *wsize, int flags);
```
- **说明**：发送聚集 IO 向量数据。
- **参数**：
  - `iov`：IO 向量数组。
  - `iovcnt`：向量条目数。
- **返回值**：同 `ccsocket_send1`。

#### `ccsocket_recv`
```c
ccsocket_stcode_t ccsocket_recv(ccsocket_t s, char *buf, size_t bsize, OPTIONAL int *rsize);
```
- **说明**：接收数据到连续缓冲区。
- **返回值**：
  - `CC_OPCODE_OK`    — 接收成功，`*rsize` 为读取字节数。
  - `CC_OPCODE_WAIT`  — 非阻塞模式下无数据可读。
  - `CC_OPCODE_ERROR` — 不可恢复错误。

#### `ccsocket_recv1`
```c
ccsocket_stcode_t ccsocket_recv1(ccsocket_t s, ccsocket_iovec_t *iov, int iovcnt, OPTIONAL int *rsize);
```
- **说明**：接收数据到 IO 向量数组。
- **返回值**：同 `ccsocket_recv`。

#### `ccsocket_peek`
```c
ccsocket_stcode_t ccsocket_peek(ccsocket_t s, char *buf, size_t bsize, OPTIONAL int *rsize);
```
- **说明**：窥视套接字接收缓冲区数据（不移除）。
- **返回值**：同 `ccsocket_recv`。

#### `ccsocket_sendfile`
```c
ccsocket_sendf_state_t ccsocket_sendfile(ccsocket_t s, int fd);
```
- **说明**：将文件描述符内容发送到套接字。
- **平台行为**：
  - macOS/FreeBSD/BSD → `sendfile()` 系统调用（零拷贝）。
  - Linux/Solaris → `sendfile()` 系统调用（零拷贝）。
  - AIX → `send_file()` 系统调用。
  - 其他（含 Windows）→ `read()` + `ccsocket_send()` 回退。
- **注意**：非一次性调用；需在循环中调用直到返回 `CC_SENDALL` 或错误。
- **返回值**：
  - `CC_SENDALL`  — 文件发送完毕。
  - `CC_SENDWAIT` — 需等待（发送缓冲区满）。
  - `CC_SENDERROR`— 不可恢复错误。

---

### 4.5 地址与名称查询

#### `ccsocket_get_peername`
```c
bool ccsocket_get_peername(ccsocket_t s, char *addr, uint16_t *port);
```
- **说明**：获取对端（远程）地址和端口。

#### `ccsocket_get_sockname`
```c
bool ccsocket_get_sockname(ccsocket_t s, char *addr, uint16_t *port);
```
- **说明**：获取本端（本地）地址和端口。

#### `ccsocket_get_family`
```c
ccsocket_family_t ccsocket_get_family(ccsocket_t s);
```
- **说明**：返回套接字地址族（`CC_INET4` / `CC_INET6` / `CC_UNIX` / `CC_FAMILY_INVALID`）。

#### `ccsocket_get_version`
```c
ccsocket_family_t ccsocket_get_version(const char *addr);
```
- **说明**：解析 IP 字符串判断其地址族（IPv4 / IPv6）。
- **参数**：`addr` 为 `NULL` 时返回 `CC_FAMILY_INVALID` 并设 `errno = EINVAL`。

#### `ccsocket_get_error`
```c
void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN]);
```
- **说明**：获取最后一个错误的可读描述。

---

### 4.6 套接字选项

#### 连接设置
```c
bool ccsocket_set_nodelay(ccsocket_t s, bool on);      // TCP_NODELAY (禁用 Nagle)
bool ccsocket_set_keepalive(ccsocket_t s, bool on);    // SO_KEEPALIVE
bool ccsocket_enable_accept_defer(ccsocket_t s);       // TCP_DEFER_ACCEPT / SO_ACCEPTFILTER
```

#### 地址复用
```c
bool ccsocket_set_reuseaddr(ccsocket_t s, bool on);    // SO_REUSEADDR
bool ccsocket_set_reuseport(ccsocket_t s, bool on);    // SO_REUSEPORT (需内核支持)
```

#### IO 行为
```c
bool ccsocket_set_nonblock(ccsocket_t s, bool on);     // 非阻塞模式
bool ccsocket_set_cloexec(ccsocket_t s, bool on);      // close-on-exec
bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout); // SO_RCVTIMEO (毫秒)
bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout); // SO_SNDTIMEO (毫秒)
```

---

### 4.7 DNS 解析

#### `ccsocket_getaddrinfo`
```c
bool ccsocket_getaddrinfo(const char *domain, ccaddrinfo_t **addrlist);
```
- **说明**：解析域名，返回去重后的地址链表。
- **参数**：
  - `domain`：域名或 IP 字符串；为 `NULL` 时返回 `false` 并设 `errno = EINVAL`。
  - `addrlist`：输出，地址链表头指针。调用者需用 `ccsocket_freeaddrinfo` 释放。
- **返回值**：`true` 成功，`false` 失败（可通过 `errno` 或 `ccsocket_get_error` 获取原因）。

#### `ccsocket_freeaddrinfo`
```c
void ccsocket_freeaddrinfo(ccaddrinfo_t *addrlist);
```
- **说明**：释放 `ccsocket_getaddrinfo` 返回的地址链表。

---

## 5. 内部辅助函数（非 ABI，仅供参考）

| 函数 | 签名 | 说明 |
|---|---|---|
| `ccsizeof` | `int ccsizeof(const struct sockaddr_storage*)` | 根据 `ss_family` 返回 sockaddr 实际大小 |
| `ccsocket2addr` | `bool ccsocket2addr(const struct sockaddr_storage*, char*, uint16_t*)` | 从 sockaddr 提取 IP 和端口 |
| `ccsocket_wrap_ip_and_port` | `int ccsocket_wrap_ip_and_port(...)` | 将 IP 字符串 + 端口填充到 sockaddr |
| `_ccsocket_set_flags` | `int _ccsocket_set_flags(ccsocket_t, ccsocket_flags_t, bool)` | 设置/清除套接字标志 |
| `_ccsocket_get_family` | `int _ccsocket_get_family(ccsocket_t, struct sockaddr_storage*)` | 获取套接字地址族 |
| `ccsocket_listen_internal` | `bool ccsocket_listen_internal(ccsocket_t, const char*, uint16_t)` | 内部 bind + listen |
| `ccsocket_recv_internal` | `ccsocket_stcode_t ccsocket_recv_internal(...)` | 内部 recv |

---

## 6. 平台兼容性矩阵

| 功能 | Linux | macOS | FreeBSD | Windows | Solaris | AIX |
|---|---|---|---|---|---|---|
| TCP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| UDP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ICMP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Unix Domain | ✅ | ✅ | ✅ | ✅[^1] | ✅ | ✅ |
| `socketpair` | ✅ | ✅ | ✅ | ✅[^2] | ✅ | ✅ |
| `sendfile` (零拷贝) | ✅ | ✅ | ✅ | ❌[^3] | ✅ | ✅ |
| `accept4` | ✅ | ❌[^4] | ❌[^4] | ❌ | ❌ | ❌ |
| `SO_REUSEPORT` | ✅ | ✅ | ✅[^5] | ❌ | ✅ | ✅ |
| `TCP_DEFER_ACCEPT` | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| `SO_ACCEPTFILTER` | ❌ | ❌ | ✅ | ❌ | ❌ | ❌ |

[^1]: Windows 10 RS2+ (1703) 原生支持，更早版本不支持。
[^2]: Windows 通过 TCP 回环模拟。
[^3]: Windows 回退为 `read()+send()`。
[^4]: macOS/FreeBSD 不支持 `accept4`，回退到 `accept()+fcntl`。
[^5]: FreeBSD 12+ 使用 `SO_REUSEPORT_LB`。

---

## 7. ccicmp API (ICMP Echo/Reply)

> 本文档由 `ccicmp.h` / `ccicmp.c` 源码自动生成。

### 7.1 `ccicmp_t` — ICMP 上下文

```c
typedef struct ccicmp_t {
    ccsocket_t fd;   // 原始套接字
    uint16_t   id;   // ICMP 标识符
    uint16_t   no;   // ICMP 序列号（每包递增）
} ccicmp_t;
```

### 7.2 `ccicmp_init`

```c
bool ccicmp_init(struct ccicmp_t *ctx, ccsocket_family_t domain);
```
- **说明**：初始化 ICMP ping 上下文，创建原始套接字。
- **参数**：
  - `ctx`：未初始化的上下文指针。
  - `domain`：`CC_INET4` 或 `CC_INET6`。
- **权限**：大多数系统需要 root / `CAP_NET_RAW`；Windows 下直接失败。
- **返回值**：`true` 成功。

### 7.3 `ccicmp_close`

```c
void ccicmp_close(struct ccicmp_t *ctx);
```
- **说明**：关闭套接字并释放上下文。

### 7.4 `ccicmp_echo`

```c
bool ccicmp_echo(struct ccicmp_t *ctx, const char *addr, const char *data, size_t len);
```
- **说明**：向目标地址发送 ICMP Echo Request。
- **参数**：
  - `addr`：目标 IPv4/IPv6 地址。
  - `data`：可选载荷（可为 `NULL`）。
  - `len`：载荷长度，上限 `CCICMP_MAX_PAYLOAD`（默认 65500，可在编译时覆盖）。
- **Payload**：每包附带 8 字节时间戳（用于 RTT 计算） + 用户数据。
- **校验和**：
  - IPv4：手动计算 ICMP 校验和（显式字节序避免 htons 歧义）。
  - IPv6：手动计算包含伪首部的 ICMPv6 校验和。
- **返回值**：`true` 发送成功（非阻塞模式下表示已提交发送）。

### 7.5 `ccicmp_reply`

```c
bool ccicmp_reply(struct ccicmp_t *ctx, char *data, size_t *len);
```
- **说明**：接收匹配的 ICMP Echo Reply。
- **参数**：
  - `data`：输出缓冲区，写入应答载荷（可为 `NULL`）。
  - `len`：输入时 = 缓冲区容量，输出时 = 实际载荷长度。
- **匹配规则**：校验 ICMP 类型（Echo Reply）、ID 和序列号是否匹配。
- **IPv6 行为**：
  - macOS/BSD：接收缓冲从 ICMPv6 头开始（无 IPv6 头）。
  - Linux：接收缓冲包含完整 IPv6 头。
- **返回值**：`true` 收到匹配的应答。

### 7.6 编译期配置宏

| 宏 | 默认值 | 说明 |
|---|---|---|
| `CCICMP_MAX_PAYLOAD` | 65500 | 最大载荷字节数（`#ifndef` 可覆盖） |
| `CCICMP_RECV_BUFSZ` | 65535 | 接收缓冲区大小（`#ifndef` 可覆盖） |
| `IPPROTO_ICMPV6` | 58 | ICMPv6 协议号（系统无此宏时自动定义） |

### 7.7 平台行为差异

| 平台 | IPv6 套接字协议 | 接收是否含 IP 头 | 校验和 |
|---|---|---|---|
| Linux | `IPPROTO_ICMPV6` (58) | 含 IPv6 头 | 手动计算 |
| macOS/BSD | `IPPROTO_ICMPV6` (58) | 无 IP 头 | 手动计算 |
| Windows | `IPPROTO_ICMPV6` (58) | — | `_ftime` 时间戳 |
