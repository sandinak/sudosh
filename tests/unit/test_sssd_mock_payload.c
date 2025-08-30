#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include "sssd_test_api.h"

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

/* Build a payload with RUNASUSER=root, OPTION=!authenticate, and two commands */
static size_t build_payload(uint8_t *buf, size_t cap)
{
    size_t off = 0;
    const char runas[] = "root";
    put_tlv(buf, cap, &off, 0x0006, runas, (uint32_t)sizeof(runas)-1);
    const char opt[] = "!authenticate";
    put_tlv(buf, cap, &off, 0x0008, opt, (uint32_t)sizeof(opt)-1);
    const char c1[] = "/bin/ls";
    put_tlv(buf, cap, &off, 0x0005, c1, (uint32_t)sizeof(c1)-1);
    const char c2[] = "!/bin/sh";
    put_tlv(buf, cap, &off, 0x0005, c2, (uint32_t)sizeof(c2)-1);
    return off;
}

int main(void)
{
    uint8_t buf[256]; size_t len = build_payload(buf, sizeof(buf));
    int rules = 0;
    int rc = sssd_parse_sudo_payload_for_test(buf, len, "tester", &rules);
    assert(rc == 0);
    assert(rules == 2);
    printf("Mock payload parsed with %d command TLVs\n", rules);
    return 0;
}

