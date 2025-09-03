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


#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <fnmatch.h>
#include <grp.h>
#include <pwd.h>

/* Local helpers mirroring production semantics (simplified for tests) */
static int sssd_command_matches(const char *pattern, const char *command)
{
    if (!pattern || !command) return 0;
    if (strcmp(pattern, "ALL") == 0) return 1;
    if (strcmp(pattern, command) == 0) return 1;
    const char *pb = strrchr(pattern, '/'); pb = pb ? pb + 1 : pattern;
    const char *cb = strrchr(command, '/'); cb = cb ? cb + 1 : command;
    if (strcmp(pb, cb) == 0) return 1;
    if (fnmatch(pattern, command, 0) == 0) return 1;
    if (fnmatch(pb, cb, 0) == 0) return 1;
    return 0;
}

static int ipv4_in_cidr(const char *ip_str, const char *cidr)
{
    const char *slash = strchr(cidr, '/'); if (!slash) return 0;
    char net[64]; size_t nlen = (size_t)(slash - cidr);
    if (nlen >= sizeof(net)) return 0;
    memcpy(net, cidr, nlen); net[nlen] = '\0';
    int prefix = atoi(slash + 1); if (prefix < 0 || prefix > 32) return 0;
    struct in_addr ip, nw; if (inet_aton(ip_str, &ip) == 0) return 0; if (inet_aton(net, &nw) == 0) return 0;
    uint32_t mask = (prefix == 0) ? 0 : htonl(0xFFFFFFFFu << (32 - prefix));
    return (ip.s_addr & mask) == (nw.s_addr & mask);
}

static int sssd_host_matches(const char *pattern, const char *shortname, const char *fqdn)
{
    if (!pattern) return 0;
    int neg = (pattern[0] == '!');
    const char *pat = neg ? pattern + 1 : pattern;
    int match = 0;
    if (strcmp(pat, "ALL") == 0) match = 1;
    else if (fnmatch(pat, shortname, 0) == 0 || fnmatch(pat, fqdn, 0) == 0) match = 1;
    else {
        /* Allow explicit IPv4 or CIDR vs provided host strings if they are IPs */
        if (inet_addr(shortname) != INADDR_NONE) {
            if (strcmp(pat, shortname) == 0 || ipv4_in_cidr(shortname, pat)) match = 1;
        }
        if (!match && inet_addr(fqdn) != INADDR_NONE) {
            if (strcmp(pat, fqdn) == 0 || ipv4_in_cidr(fqdn, pat)) match = 1;
        }
    }
    return neg ? !match : match;
}

static int sssd_user_in_group(const char *username, const char *groupname)
{
    if (!username || !groupname) return 0;
    struct group *gr = getgrnam(groupname);
    if (!gr) return 0;
    if (gr->gr_mem) { for (char **m = gr->gr_mem; *m; ++m) { if (strcmp(*m, username) == 0) return 1; } }
    struct passwd *pw = getpwnam(username);
    if (pw && gr->gr_gid == pw->pw_gid) return 1;
    return 0;
}

struct sss_sudo_rule {
    const char *user;
    const char *host;
    const char *runas_user;
    const char *runas_group;
    const char *command; /* single command per rule node */
    long order;
    time_t not_before;
    time_t not_after;
    /* options (test-only mirror) */
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
    const char *env_keep;
    const char *env_check;
    const char *env_delete;
    struct sss_sudo_rule *next;
};

static void sssd_insert_sorted_by_order(struct sss_sudo_rule **head, struct sss_sudo_rule *nr)
{
    if (!head || !nr) return;
    if (!*head) { *head = nr; return; }
    long order = nr->order;
    struct sss_sudo_rule *prev = NULL, *cur = *head;
    while (cur) {
        long curord = cur->order;
        long a = (order < 0) ? LONG_MAX : order;
        long b = (curord < 0) ? LONG_MAX : curord;
        if (a < b) break;
        prev = cur;
        cur = cur->next;
    }
    if (!prev) { nr->next = *head; *head = nr; }
    else { nr->next = cur; prev->next = nr; }
}


/* Forward decls from sssd.c used here */
/* (Removed duplicate forward decls; we implement local test versions above) */

static struct sss_sudo_rule * build_list_from_test_rules(const sssd_test_rule *rules, size_t n)
{
    struct sss_sudo_rule *head = NULL;
    for (size_t i = 0; i < n; ++i) {
        const sssd_test_rule *tr = &rules[i];
        if (!tr->commands) continue;
        for (size_t j = 0; tr->commands[j]; ++j) {
            struct sss_sudo_rule *nr = (struct sss_sudo_rule*)calloc(1, sizeof(*nr));
            if (!nr) continue;
            nr->user = tr->user; nr->host = tr->host; nr->runas_user = tr->runas_user; nr->runas_group = tr->runas_group;
            nr->command = tr->commands[j]; nr->order = tr->order; nr->not_before = tr->not_before; nr->not_after = tr->not_after;
            /* copy options from test rule */
            nr->env_reset = tr->env_reset;
            nr->setenv_allow = tr->setenv_allow;
            nr->noexec = tr->noexec;
            nr->requiretty = tr->requiretty;
            nr->lecture = tr->lecture;
            nr->log_input = tr->log_input;
            nr->log_output = tr->log_output;
            nr->umask_value = tr->umask_value;
            nr->timestamp_timeout = tr->timestamp_timeout;
            nr->verifypw = tr->verifypw;
            nr->secure_path = tr->secure_path;
            nr->cwd = tr->cwd;
            nr->chroot_dir = tr->chroot_dir;
            nr->env_keep = (char*)tr->env_keep;
            nr->env_check = (char*)tr->env_check;
            nr->env_delete = (char*)tr->env_delete;
            sssd_insert_sorted_by_order(&head, nr);
        }
    }
    return head;
}

static int rule_applies(const struct sss_sudo_rule *r, const char *username, const char *short_host, const char *fqdn, const char *runas_user, const char *runas_group)
{
    if (!r) return 0;
    time_t nowt = time(NULL);
    if (r->not_before && nowt < r->not_before) return 0;
    if (r->not_after && nowt > r->not_after) return 0;
    if (r->user && strcmp(r->user, "ALL") != 0 && strcmp(r->user, username) != 0) {
        if (r->user[0] == '%') { if (!sssd_user_in_group(username, r->user + 1)) return 0; }
        else return 0;
    }
    if (r->host && !sssd_host_matches(r->host, short_host, fqdn)) return 0;
    const char *tr_u = (runas_user && runas_user[0]) ? runas_user : "root";
    if (r->runas_user && r->runas_user[0]) { if (strcmp(r->runas_user, "ALL") != 0 && strcmp(r->runas_user, tr_u) != 0) return 0; }
    if (r->runas_group && r->runas_group[0]) {
        if (strcmp(r->runas_group, "ALL") != 0) {
            if (!runas_group || strcmp(r->runas_group, runas_group) != 0) return 0;
        }
    }
    return 1;
}

int sssd_eval_rules_for_test(const sssd_test_rule *rules, size_t n_rules,
                             const char *username, const char *short_host, const char *fqdn,
                             const char *runas_user, const char *runas_group,
                             const char *command)
{
    if (!rules || !username || !command || !short_host || !fqdn) return 0;
    struct sss_sudo_rule *list = build_list_from_test_rules(rules, n_rules);
    int any_positive = 0, any_negative = 0;
    for (struct sss_sudo_rule *r = list; r; r = r->next) {
        if (!r->command) continue;
        if (!rule_applies(r, username, short_host, fqdn, runas_user, runas_group)) continue;
        const char *pat = r->command; int is_neg = (pat[0] == '!'); if (is_neg) pat++;
        if (!is_neg && strcmp(pat, "ALL") == 0) { any_positive = 1; continue; }
        if (sssd_command_matches(pat, command)) { if (is_neg) any_negative = 1; else any_positive = 1; }
    }
    /* Free list nodes (const strings are owned by test arrays) */
    while (list) { struct sss_sudo_rule *n = list->next; free(list); list = n; }
    if (any_negative) return 0;
    return any_positive ? 1 : 0;

}

int sssd_compute_effective_options_for_test(const sssd_test_rule *rules, size_t n_rules,
                                            const char *username, const char *short_host, const char *fqdn,
                                            const char *runas_user, const char *runas_group,
                                            const char *command,
                                            sssd_effective_opts_test *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->umask_value = -1; out->timestamp_timeout = -1; out->verifypw = 0;
    if (!rules || !username || !command || !short_host || !fqdn) return 0;

    struct sss_sudo_rule *list = build_list_from_test_rules(rules, n_rules);
    int allowed = 0;
    for (struct sss_sudo_rule *r = list; r; r = r->next) {
        if (!r->command) continue;
        if (!rule_applies(r, username, short_host, fqdn, runas_user, runas_group)) continue;
        const char *pat = r->command; int is_neg = (pat[0] == '!'); if (is_neg) pat++;
        if (is_neg) {
            if (sssd_command_matches(pat, command)) { allowed = 0; break; }
            continue;
        }
        if (!allowed) {
            if (strcmp(pat, "ALL") == 0 || sssd_command_matches(pat, command)) allowed = 1;
        }
        /* Accumulate options (union semantics, last non-negative wins for specific values) */
        if (r->env_reset) out->env_reset = 1;
        if (r->setenv_allow) out->setenv_allow = 1;
        if (r->noexec) out->noexec = 1;
        if (r->requiretty) out->requiretty = 1;
        if (r->lecture) out->lecture = 1;
        if (r->log_input) out->log_input = 1;
        if (r->log_output) out->log_output = 1;
        if (r->umask_value >= 0) out->umask_value = r->umask_value;
        if (r->timestamp_timeout >= 0) out->timestamp_timeout = r->timestamp_timeout;
        if (r->verifypw) out->verifypw = r->verifypw;
        if (!out->secure_path && r->secure_path) out->secure_path = r->secure_path;
        if (!out->cwd && r->cwd) out->cwd = r->cwd;
        if (!out->chroot_dir && r->chroot_dir) out->chroot_dir = r->chroot_dir;
        if (!out->env_keep && r->env_keep) out->env_keep = r->env_keep;
        if (!out->env_check && r->env_check) out->env_check = r->env_check;
        if (!out->env_delete && r->env_delete) out->env_delete = r->env_delete;
    }
    while (list) { struct sss_sudo_rule *n = list->next; free(list); list = n; }
    return allowed;
}
