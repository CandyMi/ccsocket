/**
 * @file ccicmp.c
 * @brief ICMP echo (ping) — implementation.
 *
 * @author CandyMi
 * @license MIT
 */

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif

#include "ccicmp.h"

#include <assert.h>
#include <string.h>
#include <errno.h>

#if _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <malloc.h>
  #include <sys/timeb.h>
  #define cc_alloca _alloca
#else
  #include <sys/types.h>
  #include <sys/socket.h>   /* SOCK_DGRAM, getpeername, getsockname */
  #include <sys/time.h>
  #include <netinet/in.h>
  #include <netinet/ip.h>   /* struct ip (needed by ip_icmp.h on FreeBSD) */
  #include <netinet/ip_icmp.h>
  #include <netinet/icmp6.h>
  #define cc_alloca __builtin_alloca   /* GCC/Clang builtin, no header needed */
  #if defined(__FreeBSD__) || defined(__APPLE__)
    #include <netinet/icmp_var.h>
  #endif
#endif

/* for C89/C90 with C99 inline support */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199409L)
  #define CC_INLINE static inline
#elif defined(__cplusplus)
  #define CC_INLINE static inline
#elif _MSC_VER >= 1200
  #define CC_INLINE static __inline
#else
  #define CC_INLINE static
#endif

/* ICMP type fallback defines */
#ifndef ICMP_ECHO
  #define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHOREPLY
  #define ICMP_ECHOREPLY 0
#endif
#ifndef ICMP6_ECHO_REQUEST
  #define ICMP6_ECHO_REQUEST 128
#endif
#ifndef ICMP6_ECHO_REPLY
  #define ICMP6_ECHO_REPLY 129
#endif
#ifndef IPPROTO_ICMPV6
  #define IPPROTO_ICMPV6 58
#endif

#define CCICMP_HEADER_LEN 8  /* ICMP echo header is always 8 bytes */
#define CCICMP_TS_LEN     8  /* timestamp payload: 8 bytes */

#ifndef CCICMP_MAX_PAYLOAD
  #define CCICMP_MAX_PAYLOAD 65500  /* safe upper bound; fits within IP_MAXPACKET */
  #if defined(_MSC_VER)
    #pragma message("CCICMP_MAX_PAYLOAD defaulting to 65500")
  #elif defined(__GNUC__) || defined(__clang__)
    #warning "CCICMP_MAX_PAYLOAD defaulting to 65500"
  #endif
#endif

#ifndef CCICMP_RECV_BUFSZ
  #define CCICMP_RECV_BUFSZ 65535  /* max IP packet size */
  #if defined(_MSC_VER)
    #pragma message("CCICMP_RECV_BUFSZ defaulting to 65535")
  #elif defined(__GNUC__) || defined(__clang__)
    #warning "CCICMP_RECV_BUFSZ defaulting to 65535"
  #endif
#endif

CC_INLINE
void ccicmp_fill_timestamp(uint8_t *timestamp)
{
#if _WIN32
  struct _timeb tv;
  _ftime(&tv);
  timestamp[0] = tv.time & 0xff;
  timestamp[1] = (tv.time >> 8) & 0xff;
  timestamp[2] = (tv.time >> 16) & 0xff;
  timestamp[3] = (tv.time >> 24) & 0xff;
  timestamp[4] = tv.millitm & 0xff;
  timestamp[5] = (tv.millitm >> 8) & 0xff;
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  timestamp[0] = tv.tv_sec & 0xff;
  timestamp[1] = (tv.tv_sec >> 8) & 0xff;
  timestamp[2] = (tv.tv_sec >> 16) & 0xff;
  timestamp[3] = (tv.tv_sec >> 24) & 0xff;
  timestamp[4] = tv.tv_usec & 0xff;
  timestamp[5] = (tv.tv_usec >> 8) & 0xff;
  timestamp[6] = (tv.tv_usec >> 16) & 0xff;
  timestamp[7] = (tv.tv_usec >> 24) & 0xff;
#endif
}

/* Detect whether the socket is SOCK_DGRAM (CC_ICMP1) or SOCK_RAW (CC_ICMP).
 * Used to select the correct I/O semantics inside echo/reply. */
#define ccicmp_is_dgram(fd) (ccsocket_get_protocol(fd) == SOCK_DGRAM)

/**
 * @brief `RFC793`:
 *
 * The checksum field is the 16 bit one's complement of the one's
 * complement sum of all 16 bit words in the header and text.
 */
CC_INLINE
uint16_t icmp_checksum_calc(const uint8_t *buffer, int length)
{
  uint32_t checksum = 0;
  const uint8_t *end = buffer + length;
  if ((length & 0x01) == 0x01) {
    end = buffer + length - 1;
    checksum += (*end) << 8;
  }
  while (buffer < end) {
    checksum += buffer[0] << 8;
    checksum += buffer[1];
    buffer += 2;
  }
  uint32_t carray = checksum >> 16;
  while (carray) {
    checksum = (checksum & 0xffff) + carray;
    carray = checksum >> 16;
  }
  return (~checksum) & 0xffff;
}

bool ccicmp_init(struct ccicmp_t *ctx, ccsocket_family_t domain)
{
  if (!ctx)
    return false;
  ctx->id = ((intptr_t)ctx) & 0xffff;
  ctx->no = 0;
  ctx->ttl = -1;
  /* Try SOCK_DGRAM (CC_ICMP1 = privilege-free ping socket) first.
   * Falls back to SOCK_RAW (CC_ICMP) when the platform or runtime
   * doesn't support it (e.g. Windows, or older kernels). */
  ctx->fd = ccsocket1(domain, CC_ICMP1, CC_NONBLOCK | CC_CLOEXEC);
  if (ctx->fd == INVALID_SOCKET)
    ctx->fd = ccsocket1(domain, CC_ICMP, CC_NONBLOCK | CC_CLOEXEC);
  if (ctx->fd != INVALID_SOCKET) {
    /* Enable TTL / Hop Limit reception via CMSG */
    if (domain == CC_INET4) {
#if defined(IP_RECVTTL)
      int on = 1;
      setsockopt(ctx->fd, IPPROTO_IP, IP_RECVTTL, (char *)&on, sizeof(on));
#endif
    } else {
#if defined(IPV6_RECVHOPLIMIT)
      int on = 1;
      setsockopt(ctx->fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, (char *)&on, sizeof(on));
#endif
    }
  }
  return ctx->fd != INVALID_SOCKET;
}

void ccicmp_close(struct ccicmp_t *ctx)
{
  assert(ctx);
  if (ctx->fd != INVALID_SOCKET)
    ccsocket_close(ctx->fd);
  ctx->fd = INVALID_SOCKET;
  ctx->id = 0; ctx->no = 0;
}

bool ccicmp_echo(struct ccicmp_t *ctx, const char *addr, const char *data, size_t len)
{
  if (!ctx || ctx->fd <= 0 || !addr)
    return false;

  if (len > CCICMP_MAX_PAYLOAD)
    len = CCICMP_MAX_PAYLOAD;

  int af = ccsocket_get_family(ctx->fd);
  if (af != CC_INET4 && af != CC_INET6)
    return false;

  /* connect socket to target (enables send/recv instead of sendto/recvfrom) */
  if (!ccsocket_connect(ctx->fd, addr, 0))
    return false;

  /* -- SOCK_DGRAM (CC_ICMP1) on Linux ping socket:
   *    kernel constructs ICMP header + checksum, send only payload. -- */
  if (ccicmp_is_dgram(ctx->fd)) {
#if defined(__linux__)
    size_t pktlen = CCICMP_TS_LEN + len;
    uint8_t *packet = (uint8_t *)cc_alloca(pktlen);
    memset(packet, 0, pktlen);
    ccicmp_fill_timestamp(packet);
    if (data && len > 0)
      memcpy(packet + CCICMP_TS_LEN, data, len);
    int wsize = 0;
    ccsocket_stcode_t state = ccsocket_send(ctx->fd, packet, pktlen, &wsize);
    return state == CC_OPCODE_OK && (size_t)wsize == pktlen;
#else
    /* macOS/BSD DGRAM: kernel adds only IP header.
     * Fall through to full ICMP header + checksum (same as RAW). */
#endif
  }

  /* -- SOCK_RAW (all platforms) + SOCK_DGRAM (macOS/BSD):
   *    build full ICMP echo packet -- */
  size_t pktlen = CCICMP_HEADER_LEN + CCICMP_TS_LEN + len;
  uint8_t *packet = (uint8_t *)cc_alloca(pktlen);
  memset(packet, 0, pktlen);

  packet[0] = (af == CC_INET4) ? ICMP_ECHO : ICMP6_ECHO_REQUEST;
  packet[1] = 0;
  *(uint16_t *)(packet + 4) = htons(ctx->id);
  *(uint16_t *)(packet + 6) = htons(ctx->no++);

  ccicmp_fill_timestamp(packet + CCICMP_HEADER_LEN);
  if (data && len > 0)
    memcpy(packet + CCICMP_HEADER_LEN + CCICMP_TS_LEN, data, len);

  if (af == CC_INET4) {
    uint16_t cksum = icmp_checksum_calc(packet, pktlen);
    packet[2] = (cksum >> 8) & 0xff;
    packet[3] = cksum & 0xff;
  } else {
    /* IPv6 pseudo-header: src(16) + dst(16) + len(4) + zeros(3) + next_hdr(1)
     *
     * Per RFC 4443 §2.3, the pseudo-header MUST use the actual IPv6 source
     * and destination addresses from the IP header.
     *
     * - getsockname() → source address (the local address of the socket).
     * - getpeername() → destination address (the connected remote target).
     *
     * On macOS raw ICMPv6 sockets, getsockname() may return :: (unspecified).
     * In that case we fall back to using the destination address for both
     * fields — correct for loopback (::1 → ::1), and accepted by most peers
     * since the transport checksum is still computed over the exchange.
     */
    struct sockaddr_in6 src, dst;
    socklen_t slen = sizeof(src), dlen = sizeof(dst);

    if (getpeername(ctx->fd, (struct sockaddr *)&dst, &dlen))
      return false;
    if (getsockname(ctx->fd, (struct sockaddr *)&src, &slen)) {
      /* getsockname failed — use destination as source fallback */
      memcpy(&src, &dst, sizeof(src));
    } else {
      /* If source is :: (unspecified, common on macOS raw sockets),
       * fall back to destination address. */
      struct in6_addr zero;
      memset(&zero, 0, sizeof(zero));
      if (memcmp(&src.sin6_addr, &zero, sizeof(zero)) == 0)
        memcpy(&src.sin6_addr, &dst.sin6_addr, sizeof(struct in6_addr));
    }

    uint32_t netlen = htonl((uint32_t)pktlen);
    uint8_t phdr[40];
    memset(phdr, 0, 40);
    memcpy(phdr,      &src.sin6_addr, 16);  /* source address */
    memcpy(phdr + 16, &dst.sin6_addr, 16);  /* destination address */
    memcpy(phdr + 32, &netlen, 4);
    phdr[39] = IPPROTO_ICMPV6;

    uint8_t *cbuf = (uint8_t *)cc_alloca(40 + pktlen);
    memcpy(cbuf, phdr, 40);
    memcpy(cbuf + 40, packet, pktlen);
    uint16_t cksum = icmp_checksum_calc(cbuf, (int)(40 + pktlen));
    packet[2] = (cksum >> 8) & 0xff;
    packet[3] = cksum & 0xff;
  }

  /* send via ccsocket macro */
  int wsize = 0;
  ccsocket_stcode_t state = ccsocket_send(ctx->fd, packet, pktlen, &wsize);
  return state == CC_OPCODE_OK && (size_t)wsize == pktlen;
}

/**
 * Parse received raw packet, skip IP header if present.
 * Returns byte offset to the ICMP header, or 0 on failure.
 */
/**
 * Parse received raw packet, skip IP header if present.
 * Returns byte offset to the ICMP header, or `(size_t)-1` on failure.
 */
CC_INLINE
size_t ccicmp_skip_ip_header(const uint8_t *buf, size_t len, int af)
{
  if (af == CC_INET4) {
    if (len < 20 || (buf[0] & 0xF0) != 0x40)
      return (size_t)-1;
    size_t ihl = (buf[0] & 0x0F) * 4;
    return (ihl >= 20 && ihl <= len) ? ihl : (size_t)-1;
  }
  if (af == CC_INET6) {
    /* Has IPv6 header? (version nibble = 6, header = 40 bytes) */
    if (len >= 40 && (buf[0] & 0xF0) == 0x60)
      return 40;
    /* BSD/macOS strip the IPv6 header; buffer starts at ICMPv6 header. */
    if (len >= CCICMP_HEADER_LEN)
      return 0;
    return (size_t)-1;
  }
  return (size_t)-1;
}

bool ccicmp_reply(struct ccicmp_t *ctx, char *data, size_t *len)
{
  if (!ctx || ctx->fd <= 0)
    return false;

  int af = ccsocket_get_family(ctx->fd);
  if (af != CC_INET4 && af != CC_INET6)
    return false;

  /* receive via ccsocket_recvmsg (supports TTL via CMSG) */
  uint8_t *buf = (uint8_t *)cc_alloca(CCICMP_RECV_BUFSZ);
  uint8_t cmsg_buf[128];
  ccsocket_iovec_t iov[1];
  ccsocket_msghdr_t msg;
  int rsize = 0;
  ccsocket_stcode_t state;

  ccsocket_init_iov(iov, 1);
  ccsocket_set_iov_buf(iov, 0, buf);
  ccsocket_set_iov_len(iov, 0, CCICMP_RECV_BUFSZ);

  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  state = ccsocket_recvmsg(ctx->fd, &msg, CC_MSG_NOFLAG);
  rsize = msg.msg_bytes;
  if (state != CC_OPCODE_OK || rsize <= 0)
    return false;

  /* Extract TTL / Hop Limit from CMSG */
  ctx->ttl = -1;
  {
    ccsocket_cmsghdr_t *cmsg;
    for (cmsg = CC_CMSG_FIRSTHDR(cmsg_buf, msg.msg_controllen);
         cmsg != NULL;
         cmsg = CC_CMSG_NXTHDR(cmsg_buf, msg.msg_controllen, cmsg)) {
      if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TTL) {
        ctx->ttl = *(int *)CC_CMSG_DATA(cmsg);
        break;
      }
#if defined(IPV6_HOPLIMIT)
      if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_HOPLIMIT) {
        ctx->ttl = *(int *)CC_CMSG_DATA(cmsg);
        break;
      }
#endif
    }
  }

  /* -- SOCK_DGRAM (CC_ICMP1) on Linux ping socket:
   *    kernel strips IP + ICMP headers, delivers payload only. -- */
  if (ccicmp_is_dgram(ctx->fd)) {
#if defined(__linux__)
    if ((size_t)rsize < CCICMP_TS_LEN)
      return false;
    size_t datalen = (size_t)rsize - CCICMP_TS_LEN;
    if (data && len && datalen > 0) {
      size_t copylen = (datalen < *len) ? datalen : *len;
      memcpy(data, buf + CCICMP_TS_LEN, copylen);
      *len = copylen;
    } else if (len) {
      *len = datalen;
    }
    return true;
#else
    /* macOS/BSD DGRAM: kernel strips IP header only.
     * ICMP header is present — fall through to RAW-style parsing. */
#endif
  }

  /* -- SOCK_RAW (all platforms) + SOCK_DGRAM (macOS/BSD):
   *    parse IP + ICMP headers, match by type/code/id -- */
  size_t off = ccicmp_skip_ip_header(buf, (size_t)rsize, af);
  if (off == (size_t)-1) {
    /* No IP header detected (e.g. macOS DGRAM where kernel strips it).
     * Buffer starts at ICMP header — use offset 0 if that's valid. */
    off = 0;
  }
  if (off + CCICMP_HEADER_LEN > (size_t)rsize)
    return false;

  /* parse ICMP header */
  uint8_t  type = buf[off];
  uint8_t  code = buf[off + 1];
  uint16_t id   = ntohs(*(uint16_t *)(buf + off + 4));

  bool match = (af == CC_INET4)
    ? (type == ICMP_ECHOREPLY && id == ctx->id && code == 0)
    : (type == ICMP6_ECHO_REPLY && id == ctx->id && code == 0);
  if (!match)
    return false;

  size_t datastart = off + CCICMP_HEADER_LEN + CCICMP_TS_LEN;
  size_t datalen  = (size_t)rsize - datastart;

  if (data && len && datalen > 0) {
    size_t copylen = (datalen < *len) ? datalen : *len;
    memcpy(data, buf + datastart, copylen);
    *len = copylen;
  } else if (len) {
    *len = datalen;
  }

  return true;
}
