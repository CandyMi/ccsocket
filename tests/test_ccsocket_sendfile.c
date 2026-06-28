/**
 * test_ccsocket_sendfile.c — sendfile zero-copy round-trip test.
 *
 * Verifies:
 *   - ccsocket_sendfile sends the full contents of a file over a socket
 *   - The received data matches the original file content byte-for-byte
 *   - Returns CC_SENDALL on completion
 *
 * Strategy: write known content to a temp file, sendfile() it through a
 * socketpair, recv() on the other end, compare.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L  /* fileno(), shutdown() for C99+clang */
#endif

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if defined(_WIN32)
  #include <winsock2.h>   /* SD_SEND */
  #include <io.h>
  #include <fcntl.h>
  #define unlink _unlink
  #define close  _close
#else
  #include <unistd.h>
  #include <sys/socket.h>  /* shutdown(), SHUT_WR */
#endif

int main(void)
{
    assert(ccsocket_init());

    const char *test_data =
        "Hello sendfile! Zero-copy file transfer test.\n"
        "Line 2: verifying multi-block sendfile works correctly.\n"
        "Line 3: end of test data.\n";
    size_t data_len = strlen(test_data);

    /* --- Create temp file --- */
    FILE *f = tmpfile();
    if (!f) {
        /* tmpfile() may fail in some CI/sandbox environments */
        printf("SKIP: tmpfile() failed — file system may be read-only\n");
        return 0;
    }

    size_t nw = fwrite(test_data, 1, data_len, f);
    assert(nw == data_len);
    rewind(f);

    /* Get the underlying fd for sendfile */
#if defined(_WIN32)
    int fd = _fileno(f);
#else
    int fd = fileno(f);
#endif
    assert(fd >= 0);

    /* --- Create socketpair for sending --- */
    ccsocket_t sv[2];
    assert(ccsocketpair(sv, CC_NOFLAG));

    /* --- Send file via sendfile (loop until complete) --- */
    ccsocket_sendf_state_t sf_state;
    int iterations = 0;
    do {
        sf_state = ccsocket_sendfile(sv[0], fd);
        iterations++;
        assert(iterations < 10000);  /* guard against infinite loops */
    } while (sf_state != CC_SENDALL && sf_state != CC_SENDERROR);

    assert(sf_state == CC_SENDALL);
    printf("  sendfile completed in %d iterations\n", iterations);

    /* Close the temp file (fd is now invalid) */
    fclose(f);

    /* --- Shutdown sender so recv sees EOF --- */
#if defined(_WIN32)
    shutdown(sv[0], SD_SEND);
#else
    shutdown(sv[0], SHUT_WR);
#endif

    /* --- Receive all data on the other end --- */
    char rx_buf[4096];
    size_t total_rx = 0;
    int rsize;
    ccsocket_stcode_t st;

    while (total_rx < sizeof(rx_buf) &&
           (st = ccsocket_recv(sv[1], rx_buf + total_rx,
                                sizeof(rx_buf) - total_rx, &rsize)) == CC_OPCODE_OK)
    {
        total_rx += (size_t)rsize;
    }

    /* --- Verify --- */
    printf("  sent %zu bytes, received %zu bytes\n", data_len, total_rx);
    assert(total_rx == data_len);
    assert(memcmp(rx_buf, test_data, data_len) == 0);
    printf("  data integrity: PASS\n");

    /* --- Cleanup --- */
    ccsocket_close(sv[0]);
    ccsocket_close(sv[1]);

    printf("=== sendfile test PASSED ===\n");
    return 0;
}
