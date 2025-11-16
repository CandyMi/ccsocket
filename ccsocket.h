#ifndef _CCSOCKET_H_
#define _CCSOCKET_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if WIN32
  #define CCSOCKET_EXPORT __declspec(dllexport)
  typedef void* SOCKET;
#else
  #define CCSOCKET_EXPORT __attribute__((visibility("default")))
  typedef int SOCKET;
#endif

#ifndef INVALID_SOCKET
  #define INVALID_SOCKET (-1)
#endif
typedef int ccsocket_t;

typedef enum {
  CC_CLOEXEC  = 1,
  CC_NONBLOCK = 2,
} ccsocket_flags_t;

typedef enum {
  CC_LOCAL = 0,
  CC_INET4 = 1,
  CC_INET6 = 2,
} ccsocket_domain_t;

typedef enum {
  CC_TCP = 1,
  CC_UDP = 2,
} ccsocket_protocol_t;

#ifdef __cplusplus
extern "C" {
#endif

/* 销毁 ccsocket */
CCSOCKET_EXPORT int ccsocket_close(ccsocket_t s);

/* 创建 ccsocket */
CCSOCKET_EXPORT ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto);
/* 创建 ccsocket */
CCSOCKET_EXPORT ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags);

/* 监听 ccsocket */
CCSOCKET_EXPORT bool ccsocket_listen(ccsocket_t s, const char ip[], uint16_t port);

/* 连接 ccsocket */
CCSOCKET_EXPORT bool ccsocket_connect(ccsocket_t s, const char ip[], uint16_t port);

/* 发送 ccsocket */
CCSOCKET_EXPORT int ccsocket_send(ccsocket_t s, const void *buf, size_t bsize);

/* 接收 ccsocket */
CCSOCKET_EXPORT int ccsocket_recv(ccsocket_t s, char *buf, size_t bsize);

/* 修改一些标准/平台特有的标志来变更交互行为 */

/* 设置非延迟发送 */
CCSOCKET_EXPORT bool ccsocket_set_nodelay(ccsocket_t s, bool on);

#ifdef __cplusplus
}
#endif
#endif