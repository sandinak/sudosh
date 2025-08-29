#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include "sssd_test_api.h"

/* Minimal TLV IDs mirrored from sssd.c for tests */
#ifndef SSS_SUDO_RUNASUSER
#define SSS_SUDO_RUNASUSER 0x0006
#endif
#ifndef SSS_SUDO_COMMAND
#define SSS_SUDO_COMMAND   0x0005
#endif
#ifndef SSS_SUDO_OPTION
#define SSS_SUDO_OPTION    0x0008
#endif

int sssd_parse_sudo_payload_for_test(const uint8_t *payload, size_t pl, const char *unused_username, int *out_rules) {
    (void)unused_username;
    if (!payload || !out_rules) return -1;
    *out_rules = 0;
    size_t pos = 0; (void)pos;
    while (pos + 8 <= pl) {
        uint32_t t_net, l_net; memcpy(&t_net, payload + pos, 4); pos += 4; memcpy(&l_net, payload + pos, 4); pos += 4;
        uint32_t t = ntohl(t_net); uint32_t l = ntohl(l_net);
        if (l > pl - pos) break;
        /* const uint8_t *val = payload + pos; */
        pos += l;
        if (t == (uint32_t)SSS_SUDO_COMMAND) {
            (*out_rules)++;
        }
    }
    return 0;
}

