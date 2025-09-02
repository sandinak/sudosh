#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>

int sssd_parse_sudo_payload_for_test(const uint8_t *payload, size_t pl, const char *username, int *out_rules);

/* Test-only rule structure for deterministic evaluation */
typedef struct sssd_test_rule {
    const char *user;        /* "ALL", literal user, or %group */
    const char *host;        /* pattern, IPv4, or CIDR; may be NULL */
    const char *runas_user;  /* "ALL" or literal; may be NULL */
    const char *runas_group; /* "ALL" or literal; may be NULL */
    const char **commands;   /* NULL-terminated array of command patterns (negatives start with '!') */
    long order;              /* -1 if unset, else ascending priority */
    time_t not_before;       /* 0 if unset */
    time_t not_after;        /* 0 if unset */
} sssd_test_rule;

/* Evaluate test rules deterministically, mirroring production semantics. Returns 1 if allowed. */
int sssd_eval_rules_for_test(const sssd_test_rule *rules, size_t n_rules,
                             const char *username, const char *short_host, const char *fqdn,
                             const char *runas_user, const char *runas_group,
                             const char *command);
