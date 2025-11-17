#include "ccsocket.h"

#include <errno.h>
// #include <stdio.h>
#include <string.h>

#if WIN32
  #include <winsock2.h>
  #include <mstcpip.h>
  #include <WS2tcpip.h>
#if defined(HAS_AF_UNIX)
  #include <afunix.h>
#endif
  #include <Windows.h>
  #pragma comment(lib, "Ws2_32.lib")
  BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
  ) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        //printf("init.\n");
        WSADATA wsaData; // 用于初始化套接字
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            WSACleanup();
            exit(1);
        }
    }
    return true;
  }
#else
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/un.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <sys/socket.h>
  typedef int SOCKET;
#endif

typedef enum {
    CCSERVER = 1,
    CCCLIENT = 2,
} cc_socket_t;

static inline
int ccsizeof(const struct sockaddr_storage *sa)
{
    switch (sa->ss_family)
    {
#if defined(HAS_AF_UNIX)
        case AF_UNIX:
            return sizeof (struct sockaddr_un);
#endif
        case AF_INET:
            return sizeof (struct sockaddr_in);
        case AF_INET6:
            return sizeof (struct sockaddr_in6);
    }
    return 0;
}

static inline 
int ccsocket_get_family(ccsocket_t s, struct sockaddr_storage* sa)
{
    socklen_t addrlen = sizeof(*sa); memset(sa, 0x0, sizeof(*sa));
#if WIN32
    WSAPROTOCOL_INFOA info; int len = sizeof(info); // WinSock使用getsockname会失败. :<
    int r = getsockopt((SOCKET)s, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&info, &len);
    sa->ss_family = info.iAddressFamily;
#else
    int r = getsockname((SOCKET)s, (struct sockaddr*)sa, (socklen_t*)&addrlen);
#endif
    return r;
}

static inline
int ccsocket_wrap_ip_and_port(ccsocket_t s, struct sockaddr_storage *sa, const char ip[], uint16_t port)
{
    int r = ccsocket_get_family(s, sa);
    if (r)
        return r;
    // 根据协议转换IP与端口
    //struct sockaddr_in*  in;
    //struct sockaddr_in6* in6;
    switch((int)sa->ss_family)
    {
#if defined(HAS_AF_UNIX)
        case AF_UNIX:
        {
            struct sockaddr_un* in = (struct sockaddr_un*)sa;
            memcpy(in->sun_path, ip, strlen(ip));
            break;
        }
#endif
        case AF_INET:
        {
            struct sockaddr_in* in = (struct sockaddr_in*)sa;
            in->sin_port = htons(port);
            if (1 != inet_pton(AF_INET, ip, &in->sin_addr))
                return -1;
            break;
        }
        case AF_INET6:
        {
            struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
            in6->sin6_port = htons(port);
            if (1 != inet_pton(AF_INET6, ip, &in6->sin6_addr))
                return -1;
            break;
        }
    }
    return 0;
}

int ccsocket_close(ccsocket_t s)
{
#if WIN32
    return closesocket(s);
#else
    return close(s);
#endif
}

/* 创建 ccsocket */
ccsocket_t ccsocket(ccsocket_domain_t domain, ccsocket_protocol_t proto)
{
    return ccsocket1(domain, proto, 0);
}

/* 创建 ccsocket 顺便设置标记 */
ccsocket_t ccsocket1(ccsocket_domain_t domain, ccsocket_protocol_t proto, ccsocket_flags_t flags)
{
    int domain_r = AF_UNSPEC;
#if defined(HAS_AF_UNIX)
    if (domain == CC_LOCAL) domain_r = AF_UNIX;
#endif
    if (domain == CC_INET4) domain_r = AF_INET;
    if (domain == CC_INET6) domain_r = AF_INET6;

    int flag_r = IPPROTO_IP;
    int proto_r = SOCK_RAW;
    if (proto == CC_TCP)
    {
        proto_r = SOCK_STREAM;
        //flag_r = IPPROTO_TCP;
    }
    if (proto == CC_UDP)
    {
        proto_r = SOCK_DGRAM;
        //flag_r = IPPROTO_UDP;
    }

    bool isset = false;
    if (flags & CC_NONBLOCK) {
#if defined(SOCK_NONBLOCK) // 避免子进程继承
        isset = true;
        proto_r |= SOCK_NONBLOCK;
#endif
    }
    if (flags & CC_CLOEXEC) {
#if defined(SOCK_CLOEXEC) // 直接
        isset = true;
        proto_r |= SOCK_CLOEXEC;
#endif
    }
    // 创建
    ccsocket_t s = socket(domain_r, proto_r, flag_r);
    if (s == INVALID_SOCKET)
        return INVALID_SOCKET;
    /**
    * 如果之前没有设置, 则再这里完成.
    * 但是在多线程环境下这不能绝对保证.
    */
    if (!isset && flags) {
        int r = 0;
#if WIN32
        u_long mode = 0;
        if (flags & CC_CLOEXEC)
            r = SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0) ? 0 : -1;
        if (flags & CC_NONBLOCK)
            r = ioctlsocket(s, FIONBIO, &mode);
#else
        if (flags & CC_CLOEXEC)
            r = fcntl(s, F_SETFD, FD_CLOEXEC | fcntl(s, F_GETFD));
        if (flags & CC_NONBLOCK)
            r = fcntl(s, F_SETFL, O_NONBLOCK | fcntl(s, F_GETFL));
#endif
        if (r == -1) {
            ccsocket_close(s);
            s = INVALID_SOCKET;
        }
  }
  return s;
}

/* 监听 ccsocket */
bool ccsocket_listen(ccsocket_t s, const char ip[], uint16_t port)
{
    errno = 0; int r = 0;
    struct sockaddr_storage sa; memset(&sa, 0x0, sizeof(sa));
    r = ccsocket_wrap_ip_and_port(s, &sa, ip, port);
    if (r)
        return false;

    r = bind(s, (const struct sockaddr*)&sa, ccsizeof(&sa));
    if (r < 0)
        return false;
    r = listen(s, SOMAXCONN);
    if (r < 0)
        return false;
    return true;
}

/* 连接 ccsocket */
bool ccsocket_connect(ccsocket_t s, const char ip[], uint16_t port)
{
    errno = 0; int r;
    struct sockaddr_storage sa;
    r = ccsocket_wrap_ip_and_port(s, &sa, ip, port);
    if (r)
        return false;
    r = connect((SOCKET)s, (const struct sockaddr*)&sa, ccsizeof(&sa));
    // printf("r = %d, errno = %d\n", r, errno);
    if (r < 0)
        return false;
    return true;
}

/* 发送 ccsocket */
int ccsocket_send(ccsocket_t s, const void *buf, size_t bsize)
{
    return send((SOCKET)s, buf, (int)bsize, 0);
}

/* 接收 ccsocket */
int ccsocket_recv(ccsocket_t s, char *buf, size_t bsize)
{
    return recv((SOCKET)s, buf, (int)bsize, 0);
}

/* 开启/关闭 nodelay */
bool ccsocket_set_nodelay(ccsocket_t s, bool on)
{
    int Enable = on ? 1 : 0;
    if(setsockopt((SOCKET)s, IPPROTO_TCP, TCP_NODELAY, (char*)&Enable, sizeof(Enable))) {
        return false;
    }
    return true;
}
