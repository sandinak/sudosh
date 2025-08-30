#include "sudosh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>
#include "sssd_replay_dev.h"

static int sssd_debug_enabled_local(void) {
    const char *e = getenv("SUDOSH_DEBUG_SSSD");
    return (e && *e == '1') ? 1 : 0;
}
static void sssd_dbg_local(const char *fmt, ...) {
    if (!sssd_debug_enabled_local()) return;
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "[sudosh][sssd] "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n"); va_end(ap);
}

int decode_c_escaped_bytes(const char *in, uint8_t *out, size_t outcap, size_t *outlen) {
    size_t o = 0;
    for (const char *p = in; *p && o < outcap; ) {
        if (*p == '"') break;
        if (*p == '\\') {
            p++;
            if (*p >= '0' && *p <= '7') {
                int val = 0, digits = 0;
                while (digits < 3 && *p >= '0' && *p <= '7') { val = (val<<3) + (*p - '0'); p++; digits++; }
                if (o < outcap) out[o++] = (uint8_t)val;
                continue;
            } else if (*p) { out[o++] = (uint8_t)*p++; continue; }
            else break;
        } else {
            out[o++] = (uint8_t)*p++;
        }
    }
    *outlen = o; return 0;
}

int parse_and_replay_strace(const char *trace_path, int fd) {
    FILE *f = fopen(trace_path, "r");
    if (!f) { sssd_dbg_local("replay: cannot open %s", trace_path); return -1; }
    struct sudo_frame frames[64]; unsigned nf = 0; memset(frames, 0, sizeof(frames));
    char line[4096]; int target_fd = -1; uint32_t expect_body = 0; int have_header = 0; uint32_t cur_len = 0, cur_cmd = 0;
    while (fgets(line, sizeof(line), f)) {
        char *p = strstr(line, "sendto("); if (!p) continue;
        int lfd = -1; if (sscanf(p+7, "%d", &lfd) != 1) continue;
        char *q = strchr(p, '"'); if (!q) continue; q++;
        char *r = strchr(q, '"'); if (!r) continue; *r = '\0';
        uint8_t buf[256]; size_t blen = 0; decode_c_escaped_bytes(q, buf, sizeof(buf), &blen);
        if (blen == 16) {
            uint32_t hlen = ((uint32_t)buf[0]) | ((uint32_t)buf[1]<<8) | ((uint32_t)buf[2]<<16) | ((uint32_t)buf[3]<<24);
            uint32_t hcmd = ((uint32_t)buf[4]) | ((uint32_t)buf[5]<<8) | ((uint32_t)buf[6]<<16) | ((uint32_t)buf[7]<<24);
            if (hcmd == 0x00000001u || hcmd == 0x000000C1u || hcmd == 0x000000C2u || hcmd == 0x00000014u) {
                target_fd = lfd; have_header = 1; expect_body = (hlen > 16) ? (hlen - 16) : 0; cur_len = hlen; cur_cmd = hcmd;
                sssd_dbg_local("replay: header fd=%d len=%u cmd=0x%x expect_body=%u", lfd, hlen, hcmd, expect_body);
                continue;
            }
        }
        if (have_header && lfd == target_fd) {
            if (nf < sizeof(frames)/sizeof(frames[0])) {
                frames[nf].len = cur_len; frames[nf].cmd = cur_cmd; frames[nf].body_len = (uint32_t)blen; frames[nf].body = malloc(blen);
                if (frames[nf].body) memcpy(frames[nf].body, buf, blen);
                sssd_dbg_local("replay: body fd=%d len=%zu (expected %u)", lfd, blen, expect_body);
                nf++;
            }
            have_header = 0; expect_body = 0; cur_len = 0; cur_cmd = 0; target_fd = -1;
        }
    }
    fclose(f);
    if (nf == 0) { sssd_dbg_local("replay: no frames parsed"); return -1; }
    for (unsigned i = 0; i < nf; i++) {
        uint8_t hdr[16];
        uint32_t len = frames[i].len; uint32_t cmd = frames[i].cmd; uint32_t z = 0;
        memcpy(&hdr[0], &len, 4); memcpy(&hdr[4], &cmd, 4); memcpy(&hdr[8], &z, 4); memcpy(&hdr[12], &z, 4);
        sssd_dbg_local("replay: send hdr len=%u cmd=0x%x", len, cmd);
        if (write(fd, hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) { sssd_dbg_local("replay: write hdr failed"); return -1; }
        if (frames[i].body_len) {
            sssd_dbg_local("replay: send body %u bytes", frames[i].body_len);
            if (write(fd, frames[i].body, frames[i].body_len) != (ssize_t)frames[i].body_len) { sssd_dbg_local("replay: write body failed"); return -1; }
        }
    }
    uint32_t rhdr[4];
    struct pollfd pfd; pfd.fd = fd; pfd.events = POLLIN; int prc = poll(&pfd, 1, 2000);
    if (prc <= 0) { sssd_dbg_local("replay: poll rc=%d revents=0x%x", prc, (unsigned)pfd.revents); return -1; }
    if (read(fd, rhdr, sizeof(rhdr)) != (ssize_t)sizeof(rhdr)) { sssd_dbg_local("replay: read hdr failed"); return -1; }
    uint32_t rlen_total = rhdr[0]; uint32_t rcmd = rhdr[1]; uint32_t rstat = rhdr[2];
    sssd_dbg_local("replay: hdr len=%u cmd=0x%x status=%u", rlen_total, rcmd, rstat);
    if (rlen_total < 16 || rstat != 0) return -1;
    uint32_t body_len = rlen_total - 16;
    uint8_t *payload = malloc(body_len);
    if (!payload) return -1;
    if (read(fd, payload, body_len) != (ssize_t)body_len) { free(payload); return -1; }
    if (sssd_debug_enabled_local()) {
        size_t max = body_len < 64 ? body_len : 64;
        fprintf(stderr, "[sudosh][sssd] replay: payload[0..%zu]=", max);
        for (size_t i = 0; i < max; i++) {
            fprintf(stderr, "%02x", payload[i]);
        }
        fprintf(stderr, "\n");
    }
    free(payload);
    return 0;
}

