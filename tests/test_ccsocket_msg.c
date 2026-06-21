/**
 * test_ccsocket_msg.c — Functional test for recvmsg / sendmsg / sendto / recvfrom.
 *
 * Verifies:
 *   - recvmsg on invalid socket returns CC_OPCODE_ERROR
 *   - sendmsg on invalid socket returns CC_OPCODE_ERROR
 *   - ccsocket_cmsghdr_t layout matches platform expectations
 *   - CC_CMSG_FIRSTHDR / CC_CMSG_NXTHDR on empty/null buffers
 *   - TCP socketpair round-trip via sendto / recvfrom (connected path)
 *   - TCP socketpair round-trip via recvmsg / sendmsg
 *   - sendto with NULL addr on connected socket
 *   - recvfrom with NULL addr/port (data-only path)
 */

#include "ccsocket.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* ====================================================================
 * recvmsg on invalid socket
 * ==================================================================== */
static void test_recvmsg_invalid(void)
{
    ccsocket_iovec_t iov[1];
    ccsocket_msghdr_t msg;
    char buf[64];

    ccsocket_init_iov(iov, 1);
    ccsocket_set_iov_len(iov, 0, sizeof(buf));
    ccsocket_set_iov_buf(iov, 0, buf);

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    assert(ccsocket_recvmsg(INVALID_SOCKET, &msg, CC_MSG_NOFLAG) == CC_OPCODE_ERROR);
}

/* ====================================================================
 * sendmsg on invalid socket
 * ==================================================================== */
static void test_sendmsg_invalid(void)
{
    ccsocket_iovec_t iov[1];
    ccsocket_msghdr_t msg;

    ccsocket_init_iov(iov, 1);
    ccsocket_set_iov_len(iov, 0, 4);
    ccsocket_set_iov_buf(iov, 0, (void *)"test");

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    assert(ccsocket_sendmsg(INVALID_SOCKET, &msg, CC_MSG_NOFLAG) == CC_OPCODE_ERROR);
}

/* ====================================================================
 * ccsocket_cmsghdr_t layout
 * ==================================================================== */
static void test_cmsghdr_layout(void)
{
    /* Must be layout-compatible with platform cmsghdr / WSACMSGHDR */
    assert(sizeof(ccsocket_cmsghdr_t) == 12);
    /* cmsg_len at offset 0, cmsg_level at 4, cmsg_type at 8 */
    {
        ccsocket_cmsghdr_t c;
        memset(&c, 0, sizeof(c));
        assert((void *)&c.cmsg_len   == (void *)&c);
        assert((void *)&c.cmsg_level == (void *)((char *)&c + 4));
        assert((void *)&c.cmsg_type  == (void *)((char *)&c + 8));
    }
}

/* ====================================================================
 * CC_CMSG macros on empty/null buffers
 * ==================================================================== */
static void test_cmsg_macros_null(void)
{
    assert(CC_CMSG_FIRSTHDR(NULL, 0) == NULL);
    assert(CC_CMSG_FIRSTHDR((void *)"x", 0) == NULL);
    assert(CC_CMSG_FIRSTHDR((void *)"x", 1) == NULL);
    assert(CC_CMSG_SPACE(0) > 0);
    assert(CC_CMSG_LEN(0) > 0);
}

/* ====================================================================
 * TCP socketpair round-trip via sendto / recvfrom
 * ==================================================================== */
static void test_tcp_sendto_recvfrom(void)
{
    ccsocket_t sv[2];
    int rsize, wsize;
    int r;

    assert(ccsocketpair(sv, CC_NOFLAG));

    const char *msg = "Hello TCP sendto!";
    size_t msglen = strlen(msg);

    /* sendto with addr=NULL on connected socketpair */
    r = ccsocket_sendto(sv[0], msg, msglen, NULL, 0, &wsize);
    assert(r == CC_OPCODE_OK);
    assert(wsize == (int)msglen);

    /* recvfrom with address capture */
    {
        char buf[128];
        char src[65] = {0};
        uint16_t src_port = 0;

        r = ccsocket_recvfrom(sv[1], buf, sizeof(buf), src, &src_port, &rsize);
        assert(r == CC_OPCODE_OK);
        assert(rsize == (int)msglen);
        assert(memcmp(buf, msg, (size_t)rsize) == 0);
        /* src may be populated for connected TCP (peer address) */
        /* Some platforms may leave it empty; we just verify no crash */
    }

    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);
}

/* ====================================================================
 * TCP socketpair round-trip via recvmsg / sendmsg
 * ==================================================================== */
static void test_tcp_recvmsg_sendmsg(void)
{
    ccsocket_t sv[2];
    int r;
    int sent_bytes;

    assert(ccsocketpair(sv, CC_NOFLAG));

    /* sendmsg (msg_name empty → send on connected socket) */
    {
        ccsocket_iovec_t iov[1];
        ccsocket_msghdr_t msg;
        const char *data = "Hello TCP sendmsg!";
        size_t dlen = strlen(data);

        ccsocket_init_iov(iov, 1);
        ccsocket_set_iov_len(iov, 0, dlen);
        ccsocket_set_iov_buf(iov, 0, (void *)data);

        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        r = ccsocket_sendmsg(sv[0], &msg, CC_MSG_NOFLAG);
        assert(r == CC_OPCODE_OK);
        assert(msg.msg_bytes == (int)dlen);
        sent_bytes = msg.msg_bytes;
    }

    /* recvmsg with address output */
    {
        ccsocket_iovec_t iov[1];
        ccsocket_msghdr_t msg;
        char buf[128];

        ccsocket_init_iov(iov, 1);
        ccsocket_set_iov_len(iov, 0, sizeof(buf));
        ccsocket_set_iov_buf(iov, 0, buf);

        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        r = ccsocket_recvmsg(sv[1], &msg, CC_MSG_NOFLAG);
        assert(r == CC_OPCODE_OK);
        assert(msg.msg_bytes == sent_bytes);
        assert(strncmp(buf, "Hello TCP sendmsg!", (size_t)msg.msg_bytes) == 0);
    }

    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);
}

/* ====================================================================
 * recvfrom with NULL addr/port (data-only path)
 * ==================================================================== */
static void test_recvfrom_null_addr(void)
{
    ccsocket_t sv[2];
    int rsize;

    assert(ccsocketpair(sv, CC_NOFLAG));

    const char *msg = "recvfrom null addr test";
    assert(ccsocket_send(sv[0], msg, strlen(msg), NULL) == CC_OPCODE_OK);

    {
        char buf[128];
        /* Both addr and port are NULL — just want the data */
        assert(ccsocket_recvfrom(sv[1], buf, sizeof(buf), NULL, NULL, &rsize) == CC_OPCODE_OK);
        assert(rsize == (int)strlen(msg));
        assert(memcmp(buf, msg, (size_t)rsize) == 0);
    }

    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);
}

/* ====================================================================
 * recvmsg with CMSG control buffer (no ancillary data expected for TCP,
 * but verifies the control buffer path doesn't crash)
 * ==================================================================== */
static void test_recvmsg_with_control(void)
{
    ccsocket_t sv[2];
    int r;

    assert(ccsocketpair(sv, CC_NOFLAG));

    /* Send a message */
    const char *msg = "cmsg test";
    assert(ccsocket_send(sv[0], msg, strlen(msg), NULL) == CC_OPCODE_OK);

    /* Receive with a CMSG buffer */
    {
        ccsocket_iovec_t iov[1];
        ccsocket_msghdr_t mhdr;
        char buf[128];
        uint8_t cbuf[128];

        ccsocket_init_iov(iov, 1);
        ccsocket_set_iov_len(iov, 0, sizeof(buf));
        ccsocket_set_iov_buf(iov, 0, buf);

        memset(&mhdr, 0, sizeof(mhdr));
        mhdr.msg_iov = iov;
        mhdr.msg_iovlen = 1;
        mhdr.msg_control = cbuf;
        mhdr.msg_controllen = sizeof(cbuf);

        r = ccsocket_recvmsg(sv[1], &mhdr, CC_MSG_NOFLAG);
        assert(r == CC_OPCODE_OK);
        assert(mhdr.msg_bytes == (int)strlen(msg));
        assert(memcmp(buf, msg, (size_t)mhdr.msg_bytes) == 0);
        /* CMSG may or may not be present; verify fields are safe */
        assert(mhdr.msg_controllen <= sizeof(cbuf));
    }

    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);
}

/* ====================================================================
 * Main
 * ==================================================================== */
int main(void)
{
    assert(ccsocket_init());

    test_recvmsg_invalid();
    test_sendmsg_invalid();
    test_cmsghdr_layout();
    test_cmsg_macros_null();
    test_tcp_sendto_recvfrom();
    test_tcp_recvmsg_sendmsg();
    test_recvfrom_null_addr();
    test_recvmsg_with_control();

    return 0;
}
