#pragma once
#include <stdint.h>
#include <stddef.h>

struct sudo_frame { uint32_t len; uint32_t cmd; uint8_t *body; uint32_t body_len; };
int decode_c_escaped_bytes(const char *in, uint8_t *out, size_t outcap, size_t *outlen);
int parse_and_replay_strace(const char *trace_path, int fd);

