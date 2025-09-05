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
    /* Optional options (test-mode only); defaults are zero/NULL meaning unset */
    int env_reset;
    int setenv_allow;
    int noexec;
    int requiretty;
    int lecture;
    int log_input;
    int log_output;
    int umask_value;        /* -1 if unset */
    int timestamp_timeout;  /* minutes; -1 if unset */
    int verifypw;           /* 0=unset, 1=always, 2=any, 3=never */
    const char *secure_path;
    const char *cwd;
    const char *chroot_dir;
    const char *env_keep;   /* comma-separated */
    const char *env_check;  /* comma-separated */
    const char *env_delete; /* comma-separated */
} sssd_test_rule;

/* Evaluate test rules deterministically, mirroring production semantics. Returns 1 if allowed. */
int sssd_eval_rules_for_test(const sssd_test_rule *rules, size_t n_rules,
                             const char *username, const char *short_host, const char *fqdn,
                             const char *runas_user, const char *runas_group,
                             const char *command);

/* Minimal mirror of effective options struct for tests */
typedef struct sssd_effective_opts_test {
    int env_reset;
    int setenv_allow;
    int noexec;
    int requiretty;
    int lecture;
    int log_input;
    int log_output;
    int umask_value;        /* -1 if unset */
    int timestamp_timeout;  /* minutes; -1 if unset */
    int verifypw;           /* 0=unset, 1=always, 2=any, 3=never */
    const char *secure_path;
    const char *cwd;
    const char *chroot_dir;
    const char *env_keep;   /* comma-separated */
    const char *env_check;  /* comma-separated */
    const char *env_delete; /* comma-separated */
} sssd_effective_opts_test;

/* Compute effective options for test rules without contacting SSSD. Returns 1 if allowed. */
int sssd_compute_effective_options_for_test(const sssd_test_rule *rules, size_t n_rules,
                                            const char *username, const char *short_host, const char *fqdn,
                                            const char *runas_user, const char *runas_group,
                                            const char *command,
                                            sssd_effective_opts_test *out);
