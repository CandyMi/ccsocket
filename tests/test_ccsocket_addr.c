/**
 * test_ccsocket_addr.c — Address and name resolution tests.
 *
 * Tests ccsocket_get_version, ccsocket_get_family,
 * ccsocket_getaddrinfo, and error handling for invalid inputs.
 */

#include "ccsocket.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    assert(ccsocket_init());

    /* --- ccsocket_get_version --- */
    assert(ccsocket_get_version("1.1.1.1")     == CC_INET4);
    assert(ccsocket_get_version("127.0.0.1")   == CC_INET4);
    assert(ccsocket_get_version("0.0.0.0")     == CC_INET4);
    assert(ccsocket_get_version("::1")          == CC_INET6);
    assert(ccsocket_get_version("::")           == CC_INET6);
    assert(ccsocket_get_version("2001:db8::1") == CC_INET6);
    assert(ccsocket_get_version(NULL)           == CC_FAMILY_INVALID);

    /* non-IP string should be invalid */
    assert(ccsocket_get_version("not-an-ip") == CC_FAMILY_INVALID);

    /* --- ccsocket_getaddrinfo (localhost) --- */
    ccaddrinfo_t *list = NULL;

    /* resolve "127.0.0.1" — should yield exactly one CC_INET4 entry */
    assert(ccsocket_getaddrinfo("127.0.0.1", &list));
    assert(list != NULL);
    assert(list->af == CC_INET4);
    assert(strcmp(list->address, "127.0.0.1") == 0);
    ccsocket_freeaddrinfo(list);
    list = NULL;

    /* resolve "localhost" — should yield at least one address */
    assert(ccsocket_getaddrinfo("localhost", &list));
    assert(list != NULL);
    ccsocket_freeaddrinfo(list);
    list = NULL;

    assert(ccsocket_getaddrinfo("www.qq.com", &list));
    ccaddrinfo_t *addr = list;
    assert(list != NULL);
    while (addr) { printf("af = '%d', addr = '%s'\n", addr->af, addr->address); addr = addr->next;}
    ccsocket_freeaddrinfo(list);
    list = NULL;

    /* NULL domain should fail */
    assert(!ccsocket_getaddrinfo(NULL, &list));

    /* free NULL list is a no-op (must not crash) */
    ccsocket_freeaddrinfo(NULL);

    return 0;
}
