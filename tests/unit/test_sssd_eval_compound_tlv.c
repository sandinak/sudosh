#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "sssd_test_api.h"

/* Mirror TLV IDs from src/sssd.c */
#define SSS_SUDO_COMMAND    0x0005
#define SSS_SUDO_RUNASUSER  0x0006
#define SSS_SUDO_RUNASGROUP 0x0007
#define SSS_SUDO_OPTION     0x0008

static void put_tlv(uint8_t *buf, size_t cap, size_t *off, uint32_t t, const void *v, uint32_t l)
{
    if (!buf || !off) return;
    if (*off + 8u + (size_t)l > cap) return;
    uint32_t tn = htonl(t), ln = htonl(l);
    memcpy(buf + *off, &tn, 4);
    memcpy(buf + *off + 4, &ln, 4);
    memcpy(buf + *off + 8, v, l);
    *off += 8u + (size_t)l;
}

static size_t build_compound_payload(uint8_t *buf, size_t cap)
{
    size_t off = 0;
    const char runas[] = "root"; put_tlv(buf, cap, &off, SSS_SUDO_RUNASUSER, runas, (uint32_t)strlen(runas));
    const char rgrp[] = "root"; put_tlv(buf, cap, &off, SSS_SUDO_RUNASGROUP, rgrp, (uint32_t)strlen(rgrp));
    const char opt1[] = "!authenticate"; put_tlv(buf, cap, &off, SSS_SUDO_OPTION, opt1, (uint32_t)strlen(opt1));
    const char opt2[] = "setenv"; put_tlv(buf, cap, &off, SSS_SUDO_OPTION, opt2, (uint32_t)strlen(opt2));
    const char c1[] = "/usr/bin/apt-get"; put_tlv(buf, cap, &off, SSS_SUDO_COMMAND, c1, (uint32_t)strlen(c1));
    const char c2[] = "!/bin/sh"; put_tlv(buf, cap, &off, SSS_SUDO_COMMAND, c2, (uint32_t)strlen(c2));
    return off;
}

int main(void)
{
    uint8_t buf[512]; size_t len = build_compound_payload(buf, sizeof(buf));
    int rules = 0;
    int rc = sssd_parse_sudo_payload_for_test(buf, len, "tester", &rules);
    assert(rc == 0);
    assert(rules == 2);
    printf("Compound TLV payload parsed with %d commands.\n", rules);
    return 0;
}

