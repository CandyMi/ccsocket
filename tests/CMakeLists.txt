# -- Test Configuration --------------------------------------------------------
# Add tests via: add_test(NAME <name> COMMAND <target>)
#
# Naming convention:
#   target = test_<module>_<scenario>   (CMake target, no slashes allowed)
#   name   = <module>/<scenario>        (CTest display name, nice grouping)

# -- Helper: create a test executable and register it -------------------------
function(ccsocket_test name source)
  string(REPLACE "/" "_" tgt "${name}")
  add_executable(${tgt} ${source})
  target_link_libraries(${tgt} PRIVATE ccsocket)
  add_test(NAME ${name} COMMAND ${tgt})
endfunction()

# -- Smoke tests: compile-and-link, no runtime dependency ---------------------
ccsocket_test(ccsocket/smoke  test_ccsocket_smoke.c)
ccsocket_test(ccicmp/smoke    test_ccicmp_smoke.c)

# -- Functional tests: loopback I/O (no external network needed) --------------
ccsocket_test(ccsocket/tcp    test_ccsocket_tcp.c)
ccsocket_test(ccsocket/udp    test_ccsocket_udp.c)
ccsocket_test(ccsocket/pair   test_ccsocket_pair.c)

# -- Address / name resolution tests ------------------------------------------
ccsocket_test(ccsocket/addr   test_ccsocket_addr.c)

# -- Socket option round-trip tests -------------------------------------------
ccsocket_test(ccsocket/opts   test_ccsocket_opts.c)

# -- recvmsg / sendmsg / sendto / recvfrom functional tests ------------------
ccsocket_test(ccsocket/msg    test_ccsocket_msg.c)

# -- Combined protocol tests: text protocol over TCP --------------------------
ccsocket_test(ccsocket/http   test_ccsocket_http.c)

# -- ICMP ping functional tests (checksum, layout, lifecycle) -----------------
ccsocket_test(ccicmp/ping     test_ccicmp_ping.c)

# -- DNS client tests (encode, decode, lifecycle) ----------------------------
ccsocket_test(ccdns/test      test_ccdns.c)

# -- sendfile zero-copy round-trip test ---------------------------------------
ccsocket_test(ccsocket/sendfile  test_ccsocket_sendfile.c)

# -- Connection state detection test (is_connected) --------------------------
ccsocket_test(ccsocket/connect-state  test_ccsocket_connect_state.c)

# -- Family / protocol enum boundary tests -----------------------------------
ccsocket_test(ccsocket/family  test_ccsocket_family.c)

# -- IPv6 loopback tests -----------------------------------------------------
ccsocket_test(ccsocket/ipv6    test_ccsocket_ipv6.c)

# -- Error path tests (INVALID_SOCKET, NULL, invalid params) -----------------
ccsocket_test(ccsocket/error   test_ccsocket_error.c)

# -- C++ embedding test: compile + link as C++ (extern "C" interop) ---------
enable_language(CXX)

add_executable(ccsocket_cxx_embed test_ccsocket_cxx_embed.cpp)
target_link_libraries(ccsocket_cxx_embed PRIVATE ccsocket)
add_test(NAME ccsocket/cxx-embed COMMAND ccsocket_cxx_embed)
