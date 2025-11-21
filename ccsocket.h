#ifndef _CCSOCKET_H_
#define _CCSOCKET_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if WIN32
    #define CCSOCKET_EXPORT __declspec(dllexport)
    typedef intptr_t ccsocket_t;
#else
    #define CCSOCKET_EXPORT __attribute__((visibility("default")))
    typedef int ccsocket_t;
    #define INVALID_SOCKET (~0)
#endif

#ifndef MAX_ADDRLEN
    #define MAX_ADDRLEN 255
#endif

#ifndef MAX_ERRORLEN
    #define MAX_ERRORLEN 255
#endif

typedef enum {
    CC_NOFLAG   = 0,
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

typedef enum {
    CC_CONNECTING = 0,
    CC_CONNECTED  = 1,
    CC_CONNERROR  = 2,
} ccsocket_conn_state_t;

typedef enum {
    CC_SENDWAIT   = 0, // 正在发送(缓冲区已满)
    CC_SENDNEXT   = 1, // 再次尝试(需再次调用)
    CC_SENDALL    = 2, // 发送完成(数据已全部发送)
    CC_SENDERROR  = 3, // 发送失败(发送期间出错)
} ccsocket_sendf_state_t;

#ifdef __cplusplus
extern "C" {
#endif

/* 销毁 ccsocket */
CCSOCKET_EXPORT int ccsocket_close(ccsocket_t s);

/* 创建 ccsocket */
CCSOCKET_EXPORT ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto);
/* 创建 ccsocket */
CCSOCKET_EXPORT ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags);

/* 准入 ccsocket */
CCSOCKET_EXPORT ccsocket_t ccsocket_accept(ccsocket_t s, ccsocket_flags_t flags);

/* 准入 ccsocket (获取地址与端口)*/
CCSOCKET_EXPORT ccsocket_t ccsocket_accept1(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port, ccsocket_flags_t flags);

/* 监听 ccsocket */
CCSOCKET_EXPORT bool ccsocket_listen(ccsocket_t s, const char *addr, uint16_t port);

/* 监听 ccsocket 实现负载均衡(仅部分平台) */
CCSOCKET_EXPORT bool ccsocket_listen1(ccsocket_t s, const char *addr, uint16_t port);

/* 连接 ccsocket */
CCSOCKET_EXPORT bool ccsocket_connect(ccsocket_t s, const char *addr, uint16_t port);

/* 检查 ccsocket */
CCSOCKET_EXPORT ccsocket_conn_state_t ccsocket_is_connected(ccsocket_t s);

/* 发送 ccsocket */
CCSOCKET_EXPORT int ccsocket_send(ccsocket_t s, const void *buf, size_t bsize);

/* 接收 ccsocket */
CCSOCKET_EXPORT int ccsocket_recv(ccsocket_t s, char *buf, size_t bsize);

/* 发送文件 ccsocket */
CCSOCKET_EXPORT ccsocket_sendf_state_t ccsocket_sendfile(ccsocket_t s, int fd);

/* ********** 下面是一些标准/平台特有的标志来变更交互行为 ********** */

/* 获取错误信息 */
CCSOCKET_EXPORT void ccsocket_get_error(ccsocket_t s, char buf[MAX_ERRORLEN]);

/* 获取本端/对端地址/端口 */
CCSOCKET_EXPORT bool ccsocket_get_peername(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port);
CCSOCKET_EXPORT bool ccsocket_get_sockname(ccsocket_t s, char addr[MAX_ADDRLEN], uint16_t *port);

/* 设置接受/发送超时 */
CCSOCKET_EXPORT bool ccsocket_set_rcvtimeout(ccsocket_t s, int timeout);
CCSOCKET_EXPORT bool ccsocket_set_sndtimeout(ccsocket_t s, int timeout);

/* 设置非延迟发送 */
CCSOCKET_EXPORT bool ccsocket_set_nodelay(ccsocket_t s, bool on);

/* 设置地址重用 */
CCSOCKET_EXPORT bool ccsocket_set_reuseaddr(ccsocket_t s, bool on);

/* 设置端口重用 */
CCSOCKET_EXPORT bool ccsocket_set_reuseport(ccsocket_t s, bool on);

/* 设置非阻塞模式 */
CCSOCKET_EXPORT bool ccsocket_set_nonblock(ccsocket_t s);

/* 设置非继承模式 */
CCSOCKET_EXPORT bool ccsocket_set_cloexec(ccsocket_t s);

#ifdef __cplusplus
}
#endif
#endif