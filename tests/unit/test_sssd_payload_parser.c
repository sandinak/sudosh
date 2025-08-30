#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "sssd_test_api.h"

static void build_minimal_command_payload(uint8_t *buf, size_t *len) {
    /* TLV: RUNASUSER="root" */
    uint32_t t = htonl(0x0006), l = htonl(4);
    memcpy(buf, &t, 4); memcpy(buf+4, &l, 4); memcpy(buf+8, "root", 4);
    size_t off = 12;
    /* TLV: COMMAND="/bin/touch" */
    t = htonl(0x0005); l = htonl(10);
    memcpy(buf+off, &t, 4); memcpy(buf+off+4, &l, 4); memcpy(buf+off+8, "/bin/touch", 10);
    *len = off + 8 + 10;
}

int main(void) {
    uint8_t buf[128]; size_t len = 0; build_minimal_command_payload(buf, &len);
    int rules = 0;
    int rc = sssd_parse_sudo_payload_for_test(buf, len, "tester", &rules);
    assert(rc == 0);
    assert(rules == 1);
    printf("OK: parsed %d rule(s)\n", rules);
    return 0;
}

