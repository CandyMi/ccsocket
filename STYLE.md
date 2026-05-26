# ccsocket 编码风格指南

本文档描述 `ccsocket` 库的编码风格约定，供贡献者和维护者参考。

---

## 1. 命名约定

### 1.1 前缀体系

| 符号类别 | 前缀 | 示例 |
|---|---|---|
| 公开 API 函数 | `ccsocket_` | `ccsocket_listen()`, `ccsocket_set_nodelay()` |
| 内部函数 | `_ccsocket_` | `_ccsocket_set_flags()`, `_ccsocket_get_family()` |
| 静态函数 / 内部辅助 | `ccsocket_` + `static` | `ccsocket_listen_internal()`, `ccsocket_wrap_ip_and_port()` |
| 类型定义 | `ccsocket_` + `_t` | `ccsocket_t`, `ccsocket_iovec_t`, `ccsocket_stcode_t` |
| 枚举常量 | `CC_` 大写 | `CC_NOFLAG`, `CC_CLOEXEC`, `CC_INET4`, `CC_TCP` |
| 宏 | `CCSOCKET_` / 全大写 | `CCSOCKET_EXPORT`, `MAX_ADDRLEN`, `OPTIONAL` |
| 子模块函数 | `cc` + 模块名 + `_` | `ccicmp_init()`, `cctls_create1()` |
| 测试函数 | `cctest_` | `cctest_sockpair()`, `cctest_check_timeout()` |

### 1.2 命名规则

- **函数/变量**: 全小写蛇形（snake_case）
  ```c
  int rsize;
  bool ccsocket_set_nodelay(ccsocket_t s, bool on);
  ```
- **类型名**: 小写蛇形 + `_t` 后缀
  ```c
  typedef struct ccsocket_iovec { ... } ccsocket_iovec_t;
  typedef int ccsocket_t;
  ```
- **枚举常量**: 全大写下划线
  ```c
  CC_OPCODE_OK    = 0,
  CC_OPCODE_WAIT  = 1,
  ```
- **宏**: 全大写下划线
  ```c
  #define MAX_ADDRLEN 255
  #define CCSOCKET_EXPORT __attribute__((visibility("default")))
  ```
- **结构体成员**: 小写蛇形
  ```c
  char  address[65];
  ccsocket_family_t af;
  struct ccaddrinfo *next;
  ```

---

## 2. 缩进与格式

### 2.1 缩进

**2 空格**，禁止使用 Tab。

```c
// ✓ 正确
if (flags & CC_NONBLOCK) {
  isset = true;
  proto_r |= SOCK_NONBLOCK;
}

// ✗ 错误
if (flags & CC_NONBLOCK) {
    isset = true;      // ← 4空格
    proto_r |= SOCK_NONBLOCK;
}
```

### 2.2 括号风格

- **函数定义**: Allman 风格（`{` 另起一行）

```c
int ccsocket_close(ccsocket_t s)
{
  return closesocket(s);
}
```

- **控制语句** (`if` / `for` / `while` / `switch`): K&R 风格（`{` 跟在行末）

```c
if (s == INVALID_SOCKET) {
  ccsocket_close(s);
  return false;
}

while (addrlist) {
  ccaddrinfo_t *addr = addrlist->next;
  free(addrlist);
  addrlist = addr;
}
```

- **空函数体**: 括号另起一行

```c
void cc_wait() {
  while (true) {
    cc_usleep(1000);
  }
}
```

### 2.3 指针星号

星号靠近类型名（左结合）或居中，但在**同一文件内保持一致**。

```c
// ✓ 左结合
void* ptr;
char* buf;
void* ccthread_realloc_t;

// ✓ 或居中
void *ptr;
char *buf;
```

### 2.4 行长度

建议不超过 100 字符。超出时在运算符或逗号处合理换行。

```c
  ccsocket_dump("WSAStartup init -> {\n\tVer: %g,\n\tmaxVer: %g,\n\t...",
    HIBYTE(wsaData.wVersion) + LOBYTE(wsaData.wVersion) * 0.1,
    wsaData.iMaxSockets, wsaData.iMaxUdpDg);
```

---

## 3. 注释风格

**统一使用 `/* */`**。不允许 `//`（行尾注释）和 `/** */`（Doxygen 风格）。

### 3.1 单行注释

```c
/* close ccsocket */
int ccsocket_close(ccsocket_t s);

/* set the receive timeout (`timeout` in milliseconds). */
CCSOCKET_EXPORT bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout);
```

枚举值前已有 `/* ... */` 块注释描述，因此值本身**不附加行尾注释**：

```c
/* Used for ccsocket_flags_t: */
typedef enum {
  CC_NOFLAG   = 0,
  CC_CLOEXEC  = 1,
  CC_NONBLOCK = 2,
} ccsocket_flags_t;
```

### 3.2 多行注释

使用 `/*` + ` *` 续行，禁止 `/**`。

```c
/* enable accept defer in listen tcp socket, becare unless you know it's behavior.
 * Support Platform: `Linux` / `FreeBSD` / `Windows`(TODO).
 */
bool ccsocket_enable_accept_defer(ccsocket_t s);
```

### 3.3 分隔注释

功能段间用星号线分隔。

```c
/* ********** Below are some settings that can be used to change the behavior of `ccsocket` ********** */
```

### 3.4 注释掉的代码

禁止使用 `//` 注释整段代码。应直接删除；若需保留参考，使用 `#if 0 ... #endif`。

```c
#if 0
/* send buffer to peer `ccsocket` */
CCSOCKET_EXPORT int ccsocket_sendto(ccsocket_t s, const void *buf, size_t bsize, ...);
#endif
```

---

## 4. 头文件组织

### 4.1 包含保护

使用单下划线风格。

```c
#ifndef CCSOCKET_H
#define CCSOCKET_H

/* ... */

#endif /* CCSOCKET_H */
```

### 4.2 头文件内容顺序

1. 包含保护
2. 平台导出宏（`CCSOCKET_EXPORT`）
3. 系统头文件 `#include`
4. 类型定义（typedef / struct / enum）
5. 便捷宏
6. 函数声明（按功能分组）
7. C++ 兼容 `extern "C"` 块
8. 结尾 `#endif`

### 4.3 C++ 兼容

```c
#ifdef __cplusplus
extern "C" {
#endif

/* 函数声明 */

#ifdef __cplusplus
}
#endif
```

---

## 5. 跨平台兼容

### 5.1 平台分派

使用 `#if _WIN32`（不是 `#ifdef WIN32`），按平台三路分派。

```c
#if _WIN32
  typedef intptr_t ccsocket_t;
#else
  typedef int ccsocket_t;
#endif
```

### 5.2 INLINE 兼容层

每个 `.c` 文件顶部定义（后续应抽取到公共头文件）：

```c
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199409L)
  #define CC_INLINE static inline
#elif defined(__cplusplus)
  #define CC_INLINE static inline
#elif _MSC_VER >= 1200
  #define CC_INLINE static __inline
#else
  #define CC_INLINE static
#endif
```

### 5.3 错误处理统一

```c
#if _WIN32
  #define ccsocket_init_errno() do {errno = 0; WSASetLastError(0);} while(0)
  #define ccsocket_is_errno(err) (WSA##err == WSAGetLastError())
  #define ccsocket_set_errno(err) do{errno = err; WSASetLastError(WSA##err);} while(0)
#else
  #define ccsocket_init_errno() errno = 0
  #define ccsocket_is_errno(err) (err == errno)
  #define ccsocket_set_errno(err) errno = err
#endif
```

### 5.4 导出符号

```c
#if _WIN32
  #define CCSOCKET_EXPORT __declspec(dllexport)
#else
  #define CCSOCKET_EXPORT __attribute__((visibility("default")))
#endif
```

---

## 6. 错误处理模式

- **布尔函数**: 返回 `true`/`false`，错误信息通过 `errno` / `WSAGetLastError()` 获取
- **状态码枚举**: 使用 `ccsocket_stcode_t`（`CC_OPCODE_OK` / `CC_OPCODE_ERROR` / `CC_OPCODE_WAIT`）传递操作结果
- **句柄返回**: 失败返回 `INVALID_SOCKET` / `NULL` / 负值
- **可选参数**: 用 `OPTIONAL` 宏标记可为 `NULL` 的指针参数
- **EINTR 重试**: 被信号中断的系统调用自动重试

```c
ccsocket_stcode_t ccsocket_sendv1(ccsocket_t s, ccsocket_iovec_t *iov,
    int iovcnt, OPTIONAL int *wsize, int flags)
{
  ccsocket_init_errno();
  /* ... 系统调用 ... */
  if (w == SOCKET_ERROR) {
    if (ccsocket_is_errno(EINTR))
      return ccsocket_sendv1(s, iov, iovcnt, wsize, flags);  /* 重试 */
    if (ccsocket_is_errno(EWOULDBLOCK))
      return CC_OPCODE_WAIT;
    return CC_OPCODE_ERROR;
  }
  if (wsize) *wsize = (int)wsz;  /* 可选输出指针的 NULL 检查 */
  return CC_OPCODE_OK;
}
```

### 防御性检查

函数入口检查参数合法性，设 `errno` 后返回。

```c
bool ccsocketpair1(ccsocket_t sv[2], ccsocket_flags_t flags)
{
  ccsocket_init_errno();
  if (!sv || flags < 0 || flags > 3) {
    ccsocket_set_errno(EINVAL);
    return false;
  }
  /* ... */
}
```

---

## 7. 内存管理

- **零初始化**: 结构体使用前用 `memset` 清零

```c
struct sockaddr_storage sa;
memset(&sa, 0x0, sizeof(sa));
```

- **栈上 IO 向量**: 使用宏初始化和访问

```c
ccsocket_iovec_t iov[1];
ccsocket_init_iov(iov, 1);
ccsocket_set_iov_len(iov, 0, bsize);
ccsocket_set_iov_buf(iov, 0, buf);
```

- **动态内存**: 使用标准 `malloc` / `free`，失败时清理已分配资源

```c
cur->next = (ccaddrinfo_t*)malloc(sizeof(ccaddrinfo_t));
if (!cur->next) {
  freeaddrinfo(addrs);
  ccsocket_freeaddrinfo(*addrlist);
  *addrlist = NULL;
  return false;
}
```

---

## 8. 测试风格

- **宏定义测试用例**: 使用 `CCSOCKET_TEST_FUNCTION`

```c
CCSOCKET_TEST_FUNCTION(cctest_sockpair, {
  ccsocket_t sv[2];
  bool ok = ccsocketpair(sv, CC_NONBLOCK | CC_CLOEXEC);
  assert(ok);
  /* ... */
})
```

- **验证**: 使用 `assert()` 检查条件
- **输出**: `printf("%02d. test case : '%s' successed.\n", ++i, __FUNCTION__)`
- **命名**: 测试函数用 `cctest_` 前缀 + 被测试功能名

---

## 9. 文件命名

| 文件 | 内容 |
|---|---|
| `ccsocket.h` / `ccsocket.c` | 核心 Socket API |
| `ccicmp.h` / `ccicmp.c` | ICMP ping 封装 |
| `cctls.h` / `cctls.c` | TLS/SSL 封装（OpenSSL） |
| `main.c` | 测试入口 + 测试用例 |
| `CMakeLists.txt` / `premake5.lua` / `xmake.lua` | 构建系统 |
| `README.md` | 项目说明 |
| `ccsocket.md` | ABI 参考文档 |
| `STYLE.md` | **本文件 — 编码风格指南** |

---

> 本文档从 ccsocket 项目实际代码中提取，所有示例均来自真实代码。
> 贡献前请确保代码符合上述约定。
