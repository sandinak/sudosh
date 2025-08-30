#pragma once
#include <stdint.h>
#include <stddef.h>
int sssd_parse_sudo_payload_for_test(const uint8_t *payload, size_t pl, const char *username, int *out_rules);

