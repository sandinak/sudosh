/**
 * sssd.c - System Security Services Daemon (SSSD) Integration
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Handles SSSD integration for enterprise authentication and
 * authorization in Active Directory and LDAP environments.
 */

#include "sudosh.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <netdb.h>


#include <poll.h>

#include "../third_party/sssd/sss_cli_min.h"

#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <fnmatch.h>
#include "sssd_replay_dev.h"
#include "sssd_test_api.h"



/* Debug logging for SSSD integration (enabled via SUDOSH_DEBUG_SSSD=1) */
static int sssd_debug_enabled = -1;
static void sssd_dbg(const char *fmt, ...) {
    if (sssd_debug_enabled == -1) {
        const char *e = getenv("SUDOSH_DEBUG_SSSD");
        sssd_debug_enabled = (e && *e == '1') ? 1 : 0;
    }
    if (!sssd_debug_enabled) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[sudosh][sssd] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* Helpers for socket parity */
static void resolve_fqdn(const char *host_in, char *fqdn_out, size_t outsz) {
    if (!host_in || !fqdn_out || outsz == 0) return;
    struct addrinfo hints; memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    struct addrinfo *res = NULL;
    int rc = getaddrinfo(host_in, NULL, &hints, &res);
    if (rc == 0 && res) {
        if (res->ai_canonname && res->ai_canonname[0] != '\0') {
            snprintf(fqdn_out, outsz, "%s", res->ai_canonname);
        } else {
            snprintf(fqdn_out, outsz, "%s", host_in);
        }
        freeaddrinfo(res);
    } else {
        snprintf(fqdn_out, outsz, "%s", host_in);
    }
}
static void hex_dump_debug(const uint8_t *buf, size_t len) {
    if (!buf || !len) return;
    if (sssd_debug_enabled != 1) return;
    size_t max = len < 64 ? len : 64;
    fprintf(stderr, "[sudosh][sssd] socket: payload[0..%zu]=", max);
    for (size_t i = 0; i < max; i++) fprintf(stderr, "%02x", buf[i]);
    fprintf(stderr, "\n");
}

/* Attribute for marking intentionally unused static functions to satisfy -Werror */
#if defined(__GNUC__) || defined(__clang__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

/* Portable memmem replacement for platforms without a prototype (e.g., macOS) */
static void *sssd_memmem(const void *haystack, size_t hay_len, const void *needle, size_t needle_len)
{
    if (!haystack || !needle || needle_len == 0 || hay_len < needle_len) return NULL;
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    size_t last = hay_len - needle_len;
    for (size_t i = 0; i <= last; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needle_len) == 0) {
            return (void *)(h + i);
        }
    }
    return NULL;
}

/* Rule context used while parsing a single SSSD sudo rule */
struct sss_rule_ctx {
    char runas_user[128];
    char runas_group[128];

    int nopasswd;
    int noexec;
    int setenv_allow;
    int env_reset;
    int log_input;
    int log_output;
    int requiretty;
    int lecture;

    int timestamp_timeout;   /* minutes, -1 unset */
    int verifypw;            /* 0 unset, 1 always, 2 any, 3 never */
    int umask_value;         /* -1 unset */

    char secure_path[256];
    char cwd[256];
    char chroot_dir[256];
    char selinux_role[64];
    char selinux_type[64];
    char apparmor_profile[64];

    char env_keep[512];
    char env_check[512];
    char env_delete[512];

    char iolog_dir[256];
    char iolog_file[128];
    char iolog_group[64];
    int  iolog_mode;         /* -1 unset */

    time_t not_before;       /* 0 unset */
    time_t not_after;        /* 0 unset */
    long   order;            /* -1 unset */

    char user[128];          /* sudoUser value if present */
    char host[256];          /* sudoHost value if present */
};

static void ctx_init_defaults(struct sss_rule_ctx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->runas_user, sizeof(ctx->runas_user), "%s", "ALL");
    ctx->runas_group[0] = '\0';
    ctx->timestamp_timeout = -1;
    ctx->verifypw = 0;
    ctx->umask_value = -1;
    ctx->iolog_mode = -1;
    ctx->order = -1;
}

static int parse_octal_mode(const char *s)
{
    if (!s || !*s) return -1;
    int mode = 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '7') return -1;
        mode = (mode << 3) + (*p - '0');
        if (mode > 07777) return -1;
    }
    return mode;
}

static long parse_long(const char *s)
{
    if (!s) return -1;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end && *end == '\0') return v;
    return -1;
}

static UNUSED time_t parse_time_string(const char *s)
{
    if (!s) return 0;
    /* Try epoch seconds */
    char *end = NULL;
    long long v = strtoll(s, &end, 10);
    if (end && *end == '\0' && v > 0) return (time_t)v;
    /* Try YYYYMMDDHHMMSS (optionally with Z) */
    size_t n = strlen(s);
    if (n == 14 || (n == 15 && s[14] == 'Z')) {
        struct tm tmv; memset(&tmv, 0, sizeof(tmv));
        char buf[16]; size_t copy = n > 14 ? 14 : n; memcpy(buf, s, copy); buf[14] = '\0';
        char y[5], mo[3], d[3], h[3], mi[3], se[3];
        memcpy(y, buf, 4); y[4]='\0'; memcpy(mo, buf+4, 2); mo[2]='\0'; memcpy(d, buf+6, 2); d[2]='\0';
        memcpy(h, buf+8, 2); h[2]='\0'; memcpy(mi, buf+10, 2); mi[2]='\0'; memcpy(se, buf+12, 2); se[2]='\0';
        tmv.tm_year = atoi(y) - 1900; tmv.tm_mon = atoi(mo) - 1; tmv.tm_mday = atoi(d);
        tmv.tm_hour = atoi(h); tmv.tm_min = atoi(mi); tmv.tm_sec = atoi(se);
        /* Treat as UTC */
        return timegm(&tmv);
    }
    return 0;
}

static void ctx_apply_option(struct sss_rule_ctx *ctx, const char *opt)
{
    if (!ctx || !opt || !*opt) return;
    /* Handle !authenticate first */
    if (strcmp(opt, "!authenticate") == 0) { ctx->nopasswd = 1; return; }
    if (strcmp(opt, "authenticate") == 0) { ctx->nopasswd = 0; return; }

    if (strcmp(opt, "noexec") == 0) { ctx->noexec = 1; return; }
    if (strcmp(opt, "setenv") == 0) { ctx->setenv_allow = 1; return; }
    if (strcmp(opt, "!setenv") == 0) { ctx->setenv_allow = 0; return; }
    if (strcmp(opt, "env_reset") == 0) { ctx->env_reset = 1; return; }
    if (strcmp(opt, "log_input") == 0) { ctx->log_input = 1; return; }
    if (strcmp(opt, "log_output") == 0) { ctx->log_output = 1; return; }
    if (strcmp(opt, "requiretty") == 0) { ctx->requiretty = 1; return; }
    if (strcmp(opt, "lecture") == 0) { ctx->lecture = 1; return; }

    const char *eq = strchr(opt, '=');
    if (!eq) return;
    size_t keylen = (size_t)(eq - opt);
    const char *val = eq + 1;
    if (keylen == 11 && strncmp(opt, "secure_path", 11) == 0) {
        snprintf(ctx->secure_path, sizeof(ctx->secure_path), "%s", val);
    } else if (keylen == 3 && strncmp(opt, "cwd", 3) == 0) {
        snprintf(ctx->cwd, sizeof(ctx->cwd), "%s", val);
    } else if (keylen == 6 && strncmp(opt, "chroot", 6) == 0) {
        snprintf(ctx->chroot_dir, sizeof(ctx->chroot_dir), "%s", val);
    } else if (keylen == 7 && strncmp(opt, "env_keep", 8) == 0) {
        snprintf(ctx->env_keep, sizeof(ctx->env_keep), "%s", val);
    } else if (keylen == 8 && strncmp(opt, "env_check", 9) == 0) {
        snprintf(ctx->env_check, sizeof(ctx->env_check), "%s", val);
    } else if (keylen == 9 && strncmp(opt, "env_delete", 10) == 0) {
        snprintf(ctx->env_delete, sizeof(ctx->env_delete), "%s", val);
    } else if (keylen == 5 && strncmp(opt, "umask", 5) == 0) {
        int m = parse_octal_mode(val); if (m >= 0) ctx->umask_value = m;
    } else if (keylen == 16 && strncmp(opt, "timestamp_timeout", 16) == 0) {
        long t = parse_long(val); if (t >= 0) ctx->timestamp_timeout = (int)t;
    } else if (keylen == 13 && strncmp(opt, "passwd_timeout", 13) == 0) {
        long t = parse_long(val); if (t >= 0) ctx->timestamp_timeout = (int)t;
    } else if (keylen == 8 && strncmp(opt, "verifypw", 8) == 0) {
        if (strcmp(val, "always") == 0) ctx->verifypw = 1;
        else if (strcmp(val, "any") == 0) ctx->verifypw = 2;
        else if (strcmp(val, "never") == 0) ctx->verifypw = 3;
    } else if (keylen == 8 && strncmp(opt, "iolog_dir", 9) == 0) {
        snprintf(ctx->iolog_dir, sizeof(ctx->iolog_dir), "%s", val);
    } else if (keylen == 9 && strncmp(opt, "iolog_file", 10) == 0) {
        snprintf(ctx->iolog_file, sizeof(ctx->iolog_file), "%s", val);
    } else if (keylen == 10 && strncmp(opt, "iolog_group", 11) == 0) {
        snprintf(ctx->iolog_group, sizeof(ctx->iolog_group), "%s", val);
    } else if (keylen == 10 && strncmp(opt, "iolog_mode", 10) == 0) {
        int m = parse_octal_mode(val); if (m >= 0) ctx->iolog_mode = m;
    } else if (keylen == 4 && strncmp(opt, "role", 4) == 0) {
        snprintf(ctx->selinux_role, sizeof(ctx->selinux_role), "%s", val);
    } else if (keylen == 4 && strncmp(opt, "type", 4) == 0) {
        snprintf(ctx->selinux_type, sizeof(ctx->selinux_type), "%s", val);
    } else if (keylen == 16 && strncmp(opt, "apparmor_profile", 16) == 0) {
        snprintf(ctx->apparmor_profile, sizeof(ctx->apparmor_profile), "%s", val);
    }
}

/* Forward declaration to allow helper to reference rule struct defined later */
struct sss_sudo_rule;

/* SSSD socket paths */
#define SSSD_NSS_SOCKET "/var/lib/sss/pipes/nss"
#define SSSD_SUDO_SOCKET "/var/lib/sss/pipes/sudo"

/* SSSD sudo protocol definitions (minimal vendored) */
/* Local rule/result structures used by sudosh */
struct sss_sudo_rule {
    /* Identities and scope */
    char *user;             /* sudoUser (original; may be ALL or patterns) */
    char *host;             /* sudoHost (original; may be ALL or patterns) */

    /* Commands */
    char *command;          /* Single command captured (backward compat) */

    /* RunAs */
    char *runas_user;       /* Primary runas user (default ALL/root) */
    char *runas_group;      /* Primary runas group (optional) */

    /* Options (parsed flags) */
    int nopasswd;           /* from !authenticate */
    int noexec;             /* disallow execve in sub-commands */
    int setenv_allow;       /* allow user to override env */
    int env_reset;          /* reset environment */
    int log_input;          /* iolog input */
    int log_output;         /* iolog output */
    int requiretty;         /* require a tty */
    int lecture;            /* show lecture */

    int timestamp_timeout;  /* minutes; -1 = unset */
    int verifypw;           /* 0=unset, 1=always, 2=any, 3=never */
    int umask_value;        /* -1 if unset, else 0..0777 */

    char *secure_path;
    char *cwd;
    char *chroot_dir;
    char *selinux_role;
    char *selinux_type;
    char *apparmor_profile;

    /* env lists (comma-separated as captured) */
    char *env_keep;
    char *env_check;
    char *env_delete;

    /* iolog settings */
    char *iolog_dir;
    char *iolog_file;
    char *iolog_group;
    int   iolog_mode; /* octal mode, e.g., 0600; -1 unset */

    /* Time bounds and order */
    time_t not_before;      /* 0 if unset */
    time_t not_after;       /* 0 if unset */
    long   order;           /* sudoOrder; -1 if unset */

    struct sss_sudo_rule *next;
};

/* Define the postdef copier now that struct is known */
static void copy_ctx_to_rule_postdef(const struct sss_rule_ctx *ctx, const char *username, struct sss_sudo_rule *rule);

/* Now that struct sss_sudo_rule is defined, provide a thin wrapper */
static void copy_ctx_to_rule(const struct sss_rule_ctx *ctx, const char *username, struct sss_sudo_rule *rule)
{
    copy_ctx_to_rule_postdef(ctx, username, rule);
}


static void copy_ctx_to_rule_postdef(const struct sss_rule_ctx *ctx, const char *username, struct sss_sudo_rule *rule)
{
    rule->runas_user = ctx->runas_user[0] ? safe_strdup(ctx->runas_user) : safe_strdup("ALL");
    rule->runas_group = ctx->runas_group[0] ? safe_strdup(ctx->runas_group) : NULL;
    rule->nopasswd = ctx->nopasswd;
    rule->noexec = ctx->noexec;
    rule->setenv_allow = ctx->setenv_allow;
    rule->env_reset = ctx->env_reset;
    rule->log_input = ctx->log_input;
    rule->log_output = ctx->log_output;
    rule->requiretty = ctx->requiretty;
    rule->lecture = ctx->lecture;
    rule->timestamp_timeout = ctx->timestamp_timeout;
    rule->verifypw = ctx->verifypw;
    rule->umask_value = ctx->umask_value;
    rule->secure_path = ctx->secure_path[0] ? safe_strdup(ctx->secure_path) : NULL;
    rule->cwd = ctx->cwd[0] ? safe_strdup(ctx->cwd) : NULL;
    rule->chroot_dir = ctx->chroot_dir[0] ? safe_strdup(ctx->chroot_dir) : NULL;
    rule->selinux_role = ctx->selinux_role[0] ? safe_strdup(ctx->selinux_role) : NULL;
    rule->selinux_type = ctx->selinux_type[0] ? safe_strdup(ctx->selinux_type) : NULL;
    rule->apparmor_profile = ctx->apparmor_profile[0] ? safe_strdup(ctx->apparmor_profile) : NULL;
    rule->env_keep = ctx->env_keep[0] ? safe_strdup(ctx->env_keep) : NULL;
    rule->env_check = ctx->env_check[0] ? safe_strdup(ctx->env_check) : NULL;
    rule->env_delete = ctx->env_delete[0] ? safe_strdup(ctx->env_delete) : NULL;
    rule->iolog_dir = ctx->iolog_dir[0] ? safe_strdup(ctx->iolog_dir) : NULL;
    rule->iolog_file = ctx->iolog_file[0] ? safe_strdup(ctx->iolog_file) : NULL;
    rule->iolog_group = ctx->iolog_group[0] ? safe_strdup(ctx->iolog_group) : NULL;
    rule->iolog_mode = ctx->iolog_mode;
    rule->not_before = ctx->not_before;
    rule->not_after = ctx->not_after;
    rule->order = ctx->order;
    /* Preserve who we queried as fallback */
    rule->user = ctx->user[0] ? safe_strdup(ctx->user) : safe_strdup(username);
    rule->host = ctx->host[0] ? safe_strdup(ctx->host) : NULL;
}

struct sss_sudo_result {
    uint32_t num_rules;
    struct sss_sudo_rule *rules;
    uint32_t error_code;
    char *error_message;
};

/* SSSD sudo responder command IDs (native-endian on wire) */
#define SSS_SUDO_GET_VERSION   0x0001
#define SSS_SUDO_GET_SUDORULES 0x00C1
#define SSS_SUDO_SET_RUNAS     0x00C2
/* Attribute type IDs (minimal set used by our client). Note: These values
 * mirror sudo’s usage for the SSSD sudo responder; they are treated as TLV types here.
 */
#define SSS_SUDO_USER          0x0001
#define SSS_SUDO_UID           0x0002
#define SSS_SUDO_GROUPS        0x0003
#define SSS_SUDO_HOSTNAME      0x0004
#define SSS_SUDO_COMMAND       0x0005
#define SSS_SUDO_RUNASUSER     0x0006
/* (duplicated block removed) */

/* Vendored minimal libsss sudo API structs to interop via dlopen (from sudo 1.9.x) */
/* Forward declarations for privilege helpers used in lib path */
static int escalate_for_sssd_query(uid_t *saved_euid);
static void drop_after_sssd_query(int escalated, uid_t saved_euid);

struct libsss_sudo_attr {
    char *name;
    char **values;
    unsigned int num_values;
};
struct libsss_sudo_rule {
    unsigned int num_attrs;
    struct libsss_sudo_attr *attrs;
};
struct libsss_sudo_result {
    unsigned int num_rules;
    struct libsss_sudo_rule *rules;
};

/* Forward decls for dev replay helpers */
int parse_and_replay_strace(const char *trace_path, int fd);

typedef int  (*lib_sss_sudo_send_recv_t)(uid_t, const char*, const char*,

                                         uint32_t*, struct libsss_sudo_result**);
/* defaults not required for now */
typedef void (*lib_sss_sudo_free_result_t)(struct libsss_sudo_result*);
    /* Define error codes early for use below */
    #ifndef SSS_SUDO_ERROR_OK
    #define SSS_SUDO_ERROR_OK           0
    #define SSS_SUDO_ERROR_NOENT        1
    #define SSS_SUDO_ERROR_FATAL        2
    #define SSS_SUDO_ERROR_NOMEM        3
    #endif
/* Exposed for unit tests */
#ifdef SUDOSH_TEST_MODE
int sssd_parse_sudo_payload_for_test(const uint8_t *payload, size_t pl, const char *unused_username, int *out_rules);
#endif


typedef int  (*lib_sss_sudo_get_values_t)(struct libsss_sudo_rule*, const char*,
                                          char***);
typedef void (*lib_sss_sudo_free_values_t)(char**);

/* Try libsss_sudo.so first; if successful, convert to our local result format */
static struct sss_sudo_result *query_sssd_sudo_rules_via_lib(const char *username) {
    const char *candidates[] = {
        "/usr/lib64/libsss_sudo.so",
        "/usr/lib64/sssd/libsss_sudo.so",
        "/usr/lib/sssd/libsss_sudo.so",
        "libsss_sudo.so",
        NULL
    };
    void *handle = NULL;
    lib_sss_sudo_send_recv_t fn_send_recv = NULL;
    lib_sss_sudo_free_result_t fn_free_result = NULL;
    lib_sss_sudo_get_values_t fn_get_values = NULL;
    lib_sss_sudo_free_values_t fn_free_values = NULL;

    for (int i = 0; candidates[i]; i++) {
        handle = dlopen(candidates[i], RTLD_LAZY);
        if (handle) break;

    }
    if (!handle) {
        sssd_dbg("libsss_sudo: not found");
        return NULL;
    }

    fn_send_recv = (lib_sss_sudo_send_recv_t)dlsym(handle, "sss_sudo_send_recv");
    fn_free_result = (lib_sss_sudo_free_result_t)dlsym(handle, "sss_sudo_free_result");
    fn_get_values = (lib_sss_sudo_get_values_t)dlsym(handle, "sss_sudo_get_values");
    fn_free_values = (lib_sss_sudo_free_values_t)dlsym(handle, "sss_sudo_free_values");
    if (!fn_send_recv || !fn_free_result || !fn_get_values || !fn_free_values) {
        sssd_dbg("libsss_sudo: missing symbols (%p %p %p %p)", (void*)fn_send_recv, (void*)fn_free_result, (void*)fn_get_values, (void*)fn_free_values);
        dlclose(handle);
        return NULL;
    }

    uint32_t sss_error = 0;
    struct libsss_sudo_result *libres = NULL;
    uid_t uid = getuid();
    char hostbuf[256];
    if (gethostname(hostbuf, sizeof(hostbuf)) != 0) {
        snprintf(hostbuf, sizeof(hostbuf), "%s", "localhost");
    }
    /* escalate euid to root if setuid is in effect, required for SSSD socket */
    uid_t saved_euid = geteuid();
    int escalated = escalate_for_sssd_query(&saved_euid);
    sssd_dbg("libsss_sudo: escalate=%d euid_before=%lu host=%s", escalated, (unsigned long)saved_euid, hostbuf);
    int rc = fn_send_recv(uid, username, hostbuf, &sss_error, &libres);
    sssd_dbg("libsss_sudo: send_recv rc=%d error=%u libres=%p", rc, (unsigned)sss_error, (void*)libres);
    drop_after_sssd_query(escalated, saved_euid);
    if (rc != 0 || sss_error != 0 || libres == NULL) {
        if (libres) fn_free_result(libres);
        dlclose(handle);
        return NULL;
    }

    /* Convert to our local result */
    struct sss_sudo_result *out = calloc(1, sizeof(struct sss_sudo_result));
    if (!out) {
        fn_free_result(libres);
        dlclose(handle);
        return NULL;
    }

    for (unsigned int i = 0; i < libres->num_rules; i++) {
        struct libsss_sudo_rule *r = &libres->rules[i];
        char **cmnds = NULL, **runasusers = NULL, **opts = NULL;
        char **runasgrps = NULL, **hosts = NULL, **users = NULL, **orders = NULL, **nb = NULL, **na = NULL;
        int ret;

        /* sudoCommand list */
        ret = fn_get_values(r, "sudoCommand", &cmnds);
        if (ret != 0) continue;
        /* sudoRunAsUser or sudoRunAs */
        ret = fn_get_values(r, "sudoRunAsUser", &runasusers);
        if (ret == ENOENT) { (void)fn_get_values(r, "sudoRunAs", &runasusers); }
        (void)fn_get_values(r, "sudoRunAsGroup", &runasgrps);
        /* sudoUser and sudoHost */
        (void)fn_get_values(r, "sudoUser", &users);
        (void)fn_get_values(r, "sudoHost", &hosts);
        /* sudoOption */
        ret = fn_get_values(r, "sudoOption", &opts);
        int nopass = 0; int noexec=0,setenv_allow=0,env_reset=0,log_in=0,log_out=0,requiretty=0,lecture=0; int umask_value=-1; int ts_timeout=-1, verifypw=0; int iolog_mode=-1;
        const char *secure_path=NULL,*cwd=NULL,*chroot_dir=NULL,*sel_role=NULL,*sel_type=NULL,*app_prof=NULL,*env_keep=NULL,*env_check=NULL,*env_delete=NULL,*iolog_dir=NULL,*iolog_file=NULL,*iolog_group=NULL;
        if (ret == 0 && opts) {
            for (char **p = opts; *p; ++p) {
                if (!*p) continue;
                if (strcmp(*p, "!authenticate") == 0) { nopass = 1; continue; }
                /* Reuse ctx parser by feeding a temp ctx */
                struct sss_rule_ctx tmp; ctx_init_defaults(&tmp);
                ctx_apply_option(&tmp, *p);
                /* accumulate */
                if (tmp.noexec) noexec = 1;
                if (tmp.setenv_allow) setenv_allow = 1;
                if (tmp.env_reset) env_reset = 1;
                if (tmp.log_input) log_in = 1;
                if (tmp.log_output) log_out = 1;
                if (tmp.requiretty) requiretty = 1;
                if (tmp.lecture) lecture = 1;
                if (tmp.umask_value >= 0) umask_value = tmp.umask_value;
                if (tmp.timestamp_timeout >= 0) ts_timeout = tmp.timestamp_timeout;
                if (tmp.verifypw) verifypw = tmp.verifypw;
                if (tmp.iolog_mode >= 0) iolog_mode = tmp.iolog_mode;
                if (tmp.secure_path[0]) secure_path = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.cwd[0]) cwd = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.chroot_dir[0]) chroot_dir = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.selinux_role[0]) sel_role = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.selinux_type[0]) sel_type = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.apparmor_profile[0]) app_prof = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.env_keep[0]) env_keep = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.env_check[0]) env_check = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.env_delete[0]) env_delete = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.iolog_dir[0]) iolog_dir = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.iolog_file[0]) iolog_file = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
                if (tmp.iolog_group[0]) iolog_group = *p + (strchr(*p,'=')? (strchr(*p,'=')-(*p)+1) : 0);
            }
        }
        /* Order and time bounds */
        long order = -1; time_t not_before = 0, not_after = 0;
        if (fn_get_values(r, "sudoOrder", &orders) == 0 && orders && orders[0]) {
            order = parse_long(orders[0]);
        }
        if (fn_get_values(r, "sudoNotBefore", &nb) == 0 && nb && nb[0]) {
            not_before = parse_time_string(nb[0]);
        }
        if (fn_get_values(r, "sudoNotAfter", &na) == 0 && na && na[0]) {
            not_after = parse_time_string(na[0]);
        }

        const char *runas = (runasusers && runasusers[0]) ? runasusers[0] : "ALL";
        const char *rungrp = (runasgrps && runasgrps[0]) ? runasgrps[0] : NULL;
        const char *uhost = (hosts && hosts[0]) ? hosts[0] : NULL;
        const char *uuser = (users && users[0]) ? users[0] : username;
        if (cmnds) {
            for (char **c = cmnds; *c; ++c) {
                struct sss_sudo_rule *nr = calloc(1, sizeof(struct sss_sudo_rule));
                if (!nr) break;
                nr->user = uuser ? safe_strdup(uuser) : safe_strdup(username);
                nr->host = uhost ? safe_strdup(uhost) : NULL;
                nr->runas_user = safe_strdup(runas);
                nr->runas_group = rungrp ? safe_strdup(rungrp) : NULL;
                nr->command = safe_strdup(*c);
                nr->nopasswd = nopass;
                nr->noexec = noexec; nr->setenv_allow = setenv_allow; nr->env_reset = env_reset;
                nr->log_input = log_in; nr->log_output = log_out; nr->requiretty = requiretty; nr->lecture = lecture;
                nr->umask_value = umask_value; nr->timestamp_timeout = ts_timeout; nr->verifypw = verifypw;
                nr->secure_path = secure_path ? safe_strdup(secure_path) : NULL;
                nr->cwd = cwd ? safe_strdup(cwd) : NULL;
                nr->chroot_dir = chroot_dir ? safe_strdup(chroot_dir) : NULL;
                nr->selinux_role = sel_role ? safe_strdup(sel_role) : NULL;
                nr->selinux_type = sel_type ? safe_strdup(sel_type) : NULL;
                nr->apparmor_profile = app_prof ? safe_strdup(app_prof) : NULL;
                nr->env_keep = env_keep ? safe_strdup(env_keep) : NULL;
                nr->env_check = env_check ? safe_strdup(env_check) : NULL;
                nr->env_delete = env_delete ? safe_strdup(env_delete) : NULL;
                nr->iolog_dir = iolog_dir ? safe_strdup(iolog_dir) : NULL;
                nr->iolog_file = iolog_file ? safe_strdup(iolog_file) : NULL;
                nr->iolog_group = iolog_group ? safe_strdup(iolog_group) : NULL;
                nr->iolog_mode = iolog_mode;
                nr->order = order; nr->not_before = not_before; nr->not_after = not_after;
                if (!out->rules) {
                    out->rules = nr;
                } else {
                    struct sss_sudo_rule *tail = out->rules;
                    while (tail->next) tail = tail->next;
                    tail->next = nr;
                }
                out->num_rules++;
            }
        }

        if (cmnds) fn_free_values(cmnds);
        if (runasusers) fn_free_values(runasusers);
        if (runasgrps) fn_free_values(runasgrps);
        if (hosts) fn_free_values(hosts);
        if (users) fn_free_values(users);
        if (orders) fn_free_values(orders);
        if (nb) fn_free_values(nb);
        if (na) fn_free_values(na);
        if (opts) fn_free_values(opts);
    }

    out->error_code = (out->num_rules > 0) ? SSS_SUDO_ERROR_OK : SSS_SUDO_ERROR_NOENT;
    fn_free_result(libres);
    dlclose(handle);
    return out;
}

#define SSS_SUDO_RUNASGROUP    0x0007
#define SSS_SUDO_OPTION        0x0008

/* SSSD sudo response codes */
#define SSS_SUDO_ERROR_OK           0
#define SSS_SUDO_ERROR_NOENT        1
#define SSS_SUDO_ERROR_FATAL        2
#define SSS_SUDO_ERROR_NOMEM        3

/**
 * Write data to socket with proper error handling
 */
static int write_to_socket(int fd, const void *data, size_t len) {
    const char *ptr = (const char *)data;
    size_t written = 0;
    while (written < len) {
        ssize_t result = write(fd, ptr + written, len - written);
        if (result < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (result == 0) {
            return -1; /* Should not happen for write */
        }
        written += (size_t)result;
    }
    sssd_dbg("socket: write complete fd=%d total=%zu", fd, len);

    return 0;
}

/**
 * Read data from socket with proper error handling
 */
static int read_from_socket(int fd, void *data, size_t len) {
    char *ptr = (char *)data;
    size_t bytes_read = 0;
    while (bytes_read < len) {
        ssize_t result = read(fd, ptr + bytes_read, len - bytes_read);
        if (result < 0) {
            if (errno == EINTR) continue;
            return -1;
        sssd_dbg("socket: read progress fd=%d total=%zu/%zu", fd, bytes_read, len);

        }
        if (result == 0) return -1; /* EOF */
        bytes_read += (size_t)result;
    }
    return 0;
}

/**
 * Check if SSSD is available and running
 */
static int is_sssd_available(void) {
    struct stat st;

    /* Check if SSSD NSS socket exists */
    if (stat(SSSD_NSS_SOCKET, &st) == 0) {
        return 1;
    }

    /* Check if SSSD sudo socket exists */
    if (stat(SSSD_SUDO_SOCKET, &st) == 0) {
        return 1;
    }

    /* Check if SSSD service is running via systemctl */
    FILE *fp = popen("systemctl is-active sssd 2>/dev/null", "r");
    if (fp) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), fp)) {
            if (strncmp(buffer, "active", 6) == 0) {
                pclose(fp);
                return 1;
            }
        }
        pclose(fp);
    }

    return 0;
}

/**
 * Get user information from SSSD
 */
/* Temporary escalation helpers for SSSD sudo socket access (requires root) */
static int escalate_for_sssd_query(uid_t *saved_euid) {
    uid_t real_uid = getuid();
    uid_t effective_uid = geteuid();
    if (effective_uid == 0) {
        return 0; /* already root */
    }
    if (real_uid == effective_uid) {
        return -1; /* not setuid, cannot escalate */
    }
    *saved_euid = effective_uid;
    if (seteuid(0) != 0) {
        return -1;
    }
    return 1;
}
static void drop_after_sssd_query(int escalated, uid_t saved_euid) {
    if (escalated == 1) {
        if (seteuid(saved_euid) != 0) {
            /* best-effort drop */
        }
    }
}

/* Native-endian sudo responder header helpers */
static int sss_sudo_send_cmd(int fd, uint32_t cmd, const void *body, uint32_t body_len) {
    uint32_t hdr[4];
    hdr[0] = 16u + body_len;
    hdr[1] = cmd;
    hdr[2] = 0;
    hdr[3] = 0;
    if (write_to_socket(fd, hdr, sizeof(hdr)) != 0) return -1;
    if (body_len > 0 && write_to_socket(fd, body, body_len) != 0) return -1;
    return 0;
}

static int sss_sudo_read_hdr(int fd, uint32_t *len_out, uint32_t *cmd_out, uint32_t *status_out) {
    uint32_t rhdr[4];
    struct pollfd pfd; pfd.fd = fd; pfd.events = POLLIN; int prc = poll(&pfd, 1, 2000);
    if (prc <= 0) { sssd_dbg("socket: poll rc=%d revents=0x%x", prc, (unsigned)pfd.revents); return -1; }
    if (read_from_socket(fd, rhdr, sizeof(rhdr)) != 0) return -1;
    *len_out = rhdr[0]; *cmd_out = rhdr[1]; *status_out = rhdr[2];
    sssd_dbg("socket: sss_hdr len=%u cmd=0x%x status=%u", *len_out, *cmd_out, *status_out);
    return 0;
}

struct user_info *get_user_info_sssd(const char *username) {
    if (!username || !is_sssd_available()) {
        return NULL;
    }

    /* For now, fall back to standard getpwnam which may use SSSD
     * if NSS is configured properly */
    return get_user_info(username);
}

/**
 * Connect to SSSD sudo socket
 */
static int connect_to_sssd_sudo(void) {
    int sock_fd;
    struct sockaddr_un addr;
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SSSD_SUDO_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}

/**
 * Query SSSD sudo rules using the SSSD sudo responder socket (no sudo -l or getent)
 * Minimal TLV protocol client, modeled after sudo's SSSD integration.
 * Note: This implementation vendors only the minimal protocol needed to list commands.
 */
static struct sss_sudo_result *query_sssd_sudo_rules(const char *username) {
    int fd = -1;
    struct sss_sudo_result *result = NULL;
    char hostname[256];

    if (!username) {
        return NULL;
    }


    const char *seg = getenv("SUDOSH_SSSD_SOCKET_SEGMENTED");
    if (seg && *seg) {
        int fd2 = connect_to_sssd_sudo();
        if (fd2 < 0) {
            result->error_code = SSS_SUDO_ERROR_NOENT;
            return result;
        }
        /* 1) GET_VERSION with body {1} */
        uint32_t one = 1;
        if (sss_sudo_send_cmd(fd2, SSS_SUDO_GET_VERSION, &one, sizeof(one)) != 0) goto seg_fail;
        {
            uint32_t l,c,s; if (sss_sudo_read_hdr(fd2, &l, &c, &s) != 0 || s != 0) goto seg_fail;
            /* Drain body if present */
            if (l > 16) { uint8_t sink[64]; size_t rem = l - 16; while (rem) { size_t ch = rem > sizeof(sink) ? sizeof(sink) : rem; if (read_from_socket(fd2, sink, ch) != 0) goto seg_fail; rem -= ch; } }
        }
        /* 2) SET_RUNAS root */
        char runas_root[9] = {0,0,0,0,'r','o','o','t',0};
        if (sss_sudo_send_cmd(fd2, SSS_SUDO_SET_RUNAS, runas_root, sizeof(runas_root)) != 0) goto seg_fail;
        { uint32_t l,c,s; if (sss_sudo_read_hdr(fd2, &l, &c, &s) != 0 || s != 0) goto seg_fail; if (l > 16) { uint8_t sink[64]; size_t rem = l - 16; while (rem) { size_t ch = rem > sizeof(sink) ? sizeof(sink) : rem; if (read_from_socket(fd2, sink, ch) != 0) goto seg_fail; rem -= ch; } } }
        /* 3) GET_SUDORULES with runas root again */
        if (sss_sudo_send_cmd(fd2, SSS_SUDO_GET_SUDORULES, runas_root, sizeof(runas_root)) != 0) goto seg_fail;
        { uint32_t l,c,s; if (sss_sudo_read_hdr(fd2, &l, &c, &s) != 0 || s != 0) goto seg_fail; if (l > 16) { uint8_t sink[64]; size_t rem = l - 16; while (rem) { size_t ch = rem > sizeof(sink) ? sizeof(sink) : rem; if (read_from_socket(fd2, sink, ch) != 0) goto seg_fail; rem -= ch; } } }
        /* 4) GET_VERSION again as seen in strace */
        if (sss_sudo_send_cmd(fd2, SSS_SUDO_GET_VERSION, &one, sizeof(one)) != 0) goto seg_fail;
        { uint32_t l,c,s; if (sss_sudo_read_hdr(fd2, &l, &c, &s) != 0 || s != 0) goto seg_fail; if (l > 16) { uint8_t sink[64]; size_t rem = l - 16; while (rem) { size_t ch = rem > sizeof(sink) ? sizeof(sink) : rem; if (read_from_socket(fd2, sink, ch) != 0) goto seg_fail; rem -= ch; } } }
        /* 5) GET_SUDORULES with username tagged body (observed 4-byte prefix + name\0) */
        char ubuf[256]; size_t ulen = snprintf(ubuf+4, sizeof(ubuf)-4, "%s", username);
        if (ulen >= sizeof(ubuf)-4) goto seg_fail;
        ubuf[4+ulen] = '\0';
        /* Keep the 4-byte prefix zero; matches payload we saw */
        memset(ubuf, 0, 4);
        if (sss_sudo_send_cmd(fd2, SSS_SUDO_GET_SUDORULES, ubuf, (uint32_t)(5+ulen)) != 0) goto seg_fail;
        {
            uint32_t l,c,s; if (sss_sudo_read_hdr(fd2, &l, &c, &s) != 0 || s != 0 || l <= 16) goto seg_fail;
            uint32_t pl = l - 16; uint8_t *payload = malloc(pl); if (!payload) goto seg_fail;
            if (read_from_socket(fd2, payload, pl) != 0) { free(payload); goto seg_fail; }
            hex_dump_debug(payload, pl);
            /* Parse TLVs */
            size_t pos = 0; char current_runas[128]; (void)snprintf(current_runas, sizeof(current_runas), "ALL");
            while (pos + 8 <= pl) {
                uint32_t t_net,l_net; memcpy(&t_net, payload + pos, 4); pos += 4; memcpy(&l_net, payload + pos, 4); pos += 4;
                uint32_t t = ntohl(t_net);
                uint32_t lvl = ntohl(l_net);
                if (lvl > pl - pos) break;
                const uint8_t *val = payload + pos; pos += lvl;
                sssd_dbg("socket: TLV t=0x%x len=%u", t, lvl);
                static struct sss_rule_ctx ctx; /* static to avoid large stack on loop */
                if (t == (uint32_t)SSS_SUDO_RUNASUSER) {
                    size_t cplen = (lvl < sizeof(ctx.runas_user)-1) ? lvl : sizeof(ctx.runas_user)-1; memcpy(ctx.runas_user, val, cplen); ctx.runas_user[cplen] = '\0';
                } else if (t == (uint32_t)SSS_SUDO_OPTION) {
                    /* Options may be a comma- or newline-separated list; be liberal */
                    size_t i = 0;
                    while (i < lvl) {
                        /* extract token */
                        char token[256]; size_t tp = 0;
                        while (i < lvl && (val[i] == ' ' || val[i] == '\n' || val[i] == '\t' || val[i] == ',')) i++;
                        while (i < lvl && tp < sizeof(token)-1 && val[i] != ',' && val[i] != '\n' && val[i] != '\0') token[tp++] = (char)val[i++];
                        token[tp] = '\0';
                        if (tp > 0) ctx_apply_option(&ctx, token);
                        while (i < lvl && (val[i] == ',' || val[i] == '\n' || val[i] == '\0')) i++;
                    }
                } else if (t == (uint32_t)SSS_SUDO_COMMAND) {
                    struct sss_sudo_rule *rule = calloc(1, sizeof(struct sss_sudo_rule));
                    if (rule) {
                        copy_ctx_to_rule(&ctx, username, rule);
                        char *cmdstr = malloc(lvl + 1); if (cmdstr) { memcpy(cmdstr, val, lvl); cmdstr[lvl] = '\0'; rule->command = cmdstr; }
                        rule->next = result->rules; result->rules = rule; result->num_rules++;
                    }
                }
            }
            /* Fallback heuristic: scan for attribute names and null-terminated values */
            if (result->num_rules == 0) {
                const char *needle_cmd = "sudoCommand";
                const char *needle_runas = "sudoRunAsUser";
                const char *needle_opt = "authenticate";
                char current_runas2[128]; (void)snprintf(current_runas2, sizeof(current_runas2), "ALL");
                int current_nopasswd2 = 0;
                size_t scan = 0;
                while (scan + 12 < pl) {
                    void *p = sssd_memmem(payload + scan, pl - scan, needle_cmd, strlen(needle_cmd) + 1);
                    void *r = sssd_memmem(payload + scan, pl - scan, needle_runas, strlen(needle_runas) + 1);
                    void *o = sssd_memmem(payload + scan, pl - scan, needle_opt, strlen(needle_opt) + 1);
                    size_t next = pl;
                    if (p) { size_t pos2 = (uint8_t*)p - payload + strlen(needle_cmd) + 1; while (pos2 < pl && payload[pos2] == '\0') pos2++; size_t start = pos2; while (pos2 < pl && payload[pos2] != '\0') pos2++; if (pos2 > start) { char *cmd = malloc(pos2 - start + 1); if (cmd) { memcpy(cmd, payload + start, pos2 - start); cmd[pos2 - start] = '\0'; struct sss_sudo_rule *rule = calloc(1, sizeof(struct sss_sudo_rule)); if (rule) { rule->user = safe_strdup(username); rule->runas_user = safe_strdup(current_runas2); rule->command = cmd; rule->nopasswd = current_nopasswd2; rule->next = result->rules; result->rules = rule; result->num_rules++; } else { free(cmd); } } } next = (uint8_t*)p - payload + 1; }
                    if (r) { size_t pos2 = (uint8_t*)r - payload + strlen(needle_runas) + 1; while (pos2 < pl && payload[pos2] == '\0') pos2++; size_t start = pos2; while (pos2 < pl && payload[pos2] != '\0') pos2++; if (pos2 > start) { size_t cplen2 = (pos2 - start) < sizeof(current_runas2)-1 ? (pos2 - start) : sizeof(current_runas2)-1; memcpy(current_runas2, payload + start, cplen2); current_runas2[cplen2] = '\0'; } size_t nr = (uint8_t*)r - payload + 1; if (nr < next) next = nr; }
                    if (o) { if ((uint8_t*)o > payload && *((uint8_t*)o - 1) == '!') current_nopasswd2 = 1; else current_nopasswd2 = 0; size_t no = (uint8_t*)o - payload + 1; if (no < next) next = no; }
                    if (next >= pl) { break; } else { scan = next; }
                }
            }
            free(payload);
            close(fd2);
            result->error_code = (result->num_rules > 0) ? SSS_SUDO_ERROR_OK : SSS_SUDO_ERROR_NOENT;
            return result;
        }
seg_fail:
        close(fd2);
        result->error_code = SSS_SUDO_ERROR_FATAL;
        return result;
    }


    /* Try libsss_sudo first for exact parity unless forced to use socket */
    if (getenv("SUDOSH_SSSD_FORCE_SOCKET") == NULL) {
        struct sss_sudo_result *libres = query_sssd_sudo_rules_via_lib(username);
        if (libres) {
            sssd_dbg("libsss_sudo: using library results num=%u", (unsigned)libres->num_rules);
            return libres;
        }
    }

    /* Prepare result container */
    result = calloc(1, sizeof(struct sss_sudo_result));
    /* Dev: If replay file is provided, use it to speak exactly like sudo */
    const char *replay = getenv("SUDOSH_SSSD_REPLAY");
    if (replay && *replay) {
        /* We need an open socket first */
        uid_t saved_euid_r = geteuid();
        int escalated_r = escalate_for_sssd_query(&saved_euid_r);
        int fd_r = connect_to_sssd_sudo();
        int r = -1;
        if (fd_r >= 0) {
            r = parse_and_replay_strace(replay, fd_r);
            close(fd_r);
        }
        drop_after_sssd_query(escalated_r, saved_euid_r);
        sssd_dbg("replay: result=%d", r);
        result->error_code = (r == 0) ? SSS_SUDO_ERROR_OK : SSS_SUDO_ERROR_FATAL;
        return result;
    }

    if (!result) return NULL;

    /* Resolve hostname */
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        (void)snprintf(hostname, sizeof(hostname), "%s", "localhost");
    }

    /* Connect to SSSD sudo responder (may require root euid) */
    uid_t saved_euid = geteuid();
    int escalated = escalate_for_sssd_query(&saved_euid);
    sssd_dbg("socket: escalate=%d euid_before=%lu", escalated, (unsigned long)saved_euid);

    fd = connect_to_sssd_sudo();
    if (fd < 0) {
        drop_after_sssd_query(escalated, saved_euid);
        result->error_code = SSS_SUDO_ERROR_NOENT;
        return result;
    }

    /*
     * Vendor-minimal TLV protocol (attribution: based on sudo's SSSD sudo responder client):
     * Request framing (all fields in network byte order):
     *   uint32_t cmd   = SSS_SUDO_GET_SUDORULES
     *   uint32_t nattrs = number of TLV attributes
     *   Repeated nattrs times:
     *       uint32_t type (e.g., SSS_SUDO_USER, SSS_SUDO_HOSTNAME, ...)
     *       uint32_t length
     *       uint8_t[length] value (no NUL terminator required)
     * Response framing:
     *   uint32_t status (SSS_SUDO_ERROR_OK,...)
     *   uint32_t length (payload length)
     *   uint8_t[length] payload TLVs containing sudo rules and options
     */

    /* Build request matching sudo parity as closely as possible */
    char reqbuf[16384];
    size_t off = 0;
    /* body begins with attr_count; command is only in SSSD header */

    /* Resolve UID */
    uid_t uid = getuid();

    /* Resolve groups for the current user (portable across Linux/macOS) */
    gid_t groups_buf[256];
    int ngroups = (int)(sizeof(groups_buf) / sizeof(groups_buf[0]));
#ifdef __APPLE__
    {
        /* macOS prototype: int getgrouplist(const char *, int, int *, int *) */
        int igroups[256];
        int ign = (int)(sizeof(igroups) / sizeof(igroups[0]));
        if (getgrouplist(username, (int)getgid(), igroups, &ign) < 0) {
            ign = 1;
            igroups[0] = (int)getgid();
        }
        ngroups = ign > 256 ? 256 : ign;
        for (int i = 0; i < ngroups; i++) groups_buf[i] = (gid_t)igroups[i];
    }
#else
    /* Linux prototype: int getgrouplist(const char *, gid_t, gid_t *, int *) */
    if (getgrouplist(username, getgid(), groups_buf, &ngroups) < 0) {
        /* If it failed, clamp to known group */
        ngroups = 1;
        groups_buf[0] = getgid();
    }
#endif

    /* Hostname and FQDN */
    char fqdn[256];
    resolve_fqdn(hostname, fqdn, sizeof(fqdn));

    /* Compute attribute count: USER, UID, GROUPS, HOSTNAME, RUNASUSER (+FQDN if different) */
    uint32_t attr_count = 5;
    if (strcmp(fqdn, hostname) != 0) {
        /* Some SSSD setups include a separate canonical host TLV; we include both */
        attr_count += 1; /* second HOSTNAME entry with FQDN */
    }

    /* Helper to append bytes safely */
    #define APPEND_DATA(ptr,len) do { \
        if (off + (len) > sizeof(reqbuf)) { goto io_error; } \
        memcpy(reqbuf + off, (ptr), (len)); \
        off += (len); \
    } while (0)

    /* attr count only (command goes in SSSD header) */
    {
        uint32_t nattrs = htonl(attr_count);
        APPEND_DATA(&nattrs, sizeof(nattrs));
    }

    /* TLV: HOSTNAME (short) */
    {
        uint32_t t = htonl((uint32_t)SSS_SUDO_HOSTNAME);
        uint32_t l = htonl((uint32_t)(strlen(hostname) + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(hostname, strlen(hostname) + 1);
    }

    /* TLV: HOSTNAME (FQDN) if different */
    if (strcmp(fqdn, hostname) != 0) {
        uint32_t t = htonl((uint32_t)SSS_SUDO_HOSTNAME);
        uint32_t l = htonl((uint32_t)(strlen(fqdn) + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(fqdn, strlen(fqdn) + 1);
    }

    /* TLV: USER */
    {
        uint32_t t = htonl((uint32_t)SSS_SUDO_USER);
        uint32_t l = htonl((uint32_t)(strlen(username) + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(username, strlen(username) + 1);
    }

    /* TLV: UID (decimal string, NUL-terminated) */
    {
        char uidstr[32];
        int ulen = snprintf(uidstr, sizeof(uidstr), "%lu", (unsigned long)uid);
        if (ulen < 0) goto io_error;
        uidstr[ulen] = '\0';
        uint32_t t = htonl((uint32_t)SSS_SUDO_UID);
        uint32_t l = htonl((uint32_t)(ulen + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(uidstr, (size_t)(ulen + 1));
    }

    /* TLV: GROUPS (comma-separated gid list, NUL-terminated) */
    {
        char gstr[2048];
        size_t gpos = 0;
        sssd_dbg("socket: building request HOST/HOSTFQDN/USER/UID/GROUPS/RUNAS count=%u", attr_count);
        for (int gi = 0; gi < ngroups; gi++) {
            char tmp[32];
            int w = snprintf(tmp, sizeof(tmp), gi == 0 ? "%lu" : ",%lu", (unsigned long)groups_buf[gi]);
            if (w < 0) goto io_error;
            if (gpos + (size_t)w >= sizeof(gstr) - 1) break;
            memcpy(gstr + gpos, tmp, (size_t)w);
            gpos += (size_t)w;
#ifdef SUDOSH_TEST_MODE
int sssd_parse_sudo_payload_for_test(const uint8_t *payload, size_t pl, const char *unused_username, int *out_rules) {
    if (!payload || !out_rules) {
        return -1;
    }
    *out_rules = 0;
    size_t pos = 0; char current_runas[64]; snprintf(current_runas, sizeof(current_runas), "ALL");
    while (pos + 8 <= pl) {
        uint32_t t_net,l_net; memcpy(&t_net, payload + pos, 4); pos += 4; memcpy(&l_net, payload + pos, 4); pos += 4;
        uint32_t t = ntohl(t_net); uint32_t l = ntohl(l_net);
        if (l > pl - pos) break;
        const uint8_t *val = payload + pos; pos += l;
        if (t == (uint32_t)SSS_SUDO_RUNASUSER) {
            size_t cplen = (l < sizeof(current_runas)-1) ? l : sizeof(current_runas)-1; memcpy(current_runas, val, cplen); current_runas[cplen] = '\0';
        } else if (t == (uint32_t)SSS_SUDO_OPTION) {
            /* ignore in test helper */
        } else if (t == (uint32_t)SSS_SUDO_COMMAND) {
            (*out_rules)++;
        }
    }
    return 0;
}
#endif /* SUDOSH_TEST_MODE */

        }
        gstr[gpos] = '\0';
        uint32_t t = htonl((uint32_t)SSS_SUDO_GROUPS);
        uint32_t l = htonl((uint32_t)(gpos + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(gstr, gpos + 1);
    }

    /* TLV: RUNASUSER (default root), NUL-terminated */
    {
        const char *runas = "root";
        uint32_t t = htonl((uint32_t)SSS_SUDO_RUNASUSER);
        uint32_t l = htonl((uint32_t)(strlen(runas) + 1));
        APPEND_DATA(&t, sizeof(t));
        APPEND_DATA(&l, sizeof(l));
        APPEND_DATA(runas, strlen(runas) + 1);
    }

    sssd_dbg("socket: write len=%zu", off);

    /* Remove GET_VERSION probe: some responders may not expect it on sudo pipe */
    (void)0;
        sssd_dbg("socket: body send size=%zu", off);

    /* Debug: dump first 64 bytes of the body we will send */
    hex_dump_debug((const uint8_t *)reqbuf, off);



    /* Send request using SSSD client protocol header */
    {
        uint32_t hdr[4];
        hdr[0] = (uint32_t)(SSS_NSS_HEADER_SIZE + off);
        /* Optional override for command ID via env (decimal or hex) */
        uint32_t cmd_id = (uint32_t)SSS_SUDO_GET_SUDORULES;
        const char *e_cmd = getenv("SUDOSH_SSSD_SUDO_CMD_ID");
        if (e_cmd && *e_cmd) {
            if (strncmp(e_cmd, "0x", 2) == 0 || strncmp(e_cmd, "0X", 2) == 0) {
                cmd_id = (uint32_t)strtoul(e_cmd, NULL, 16);
            } else {
                cmd_id = (uint32_t)strtoul(e_cmd, NULL, 10);
            }
        }
        /* sudo responder expects native byte order header fields */
        hdr[1] = cmd_id;
        hdr[2] = 0;
        hdr[3] = 0;




        sssd_dbg("socket: write header+body ok; waiting for reply header (%zu bytes)", sizeof(uint32_t) * 4);

        if (write_to_socket(fd, hdr, sizeof(hdr)) != 0) {
            sssd_dbg("socket: wrote header %u bytes", (unsigned)sizeof(hdr));

            sssd_dbg("socket: write header failed");
            goto io_error;
        }
        if (write_to_socket(fd, reqbuf, off) != 0) {
            sssd_dbg("socket: wrote body %zu bytes", off);

            sssd_dbg("socket: write body failed");
        /* Wait up to 2 seconds for readable data */
        struct pollfd pfd; pfd.fd = fd; pfd.events = POLLIN;
        int prc = poll(&pfd, 1, 2000);
        if (prc <= 0) {
            sssd_dbg("socket: poll for reply header rc=%d revents=0x%x errno=%d (%s)", prc, (unsigned)pfd.revents, errno, strerror(errno));
            goto io_error;
        }

            goto io_error;
        sssd_dbg("socket: waiting to read reply header (%zu bytes)", sizeof(uint32_t) * 4);
        }
        /* Read SSSD reply header */
        uint32_t rhdr[4];
        if (read_from_socket(fd, rhdr, sizeof(rhdr)) != 0) {
            sssd_dbg("socket: read reply header failed: errno=%d (%s)", errno, strerror(errno));

            sssd_dbg("socket: read reply header failed");
            goto io_error;
        }
        /* sudo responder uses native-endian header fields */
        uint32_t rlen_total = rhdr[0];
        uint32_t rcmd = rhdr[1];
        uint32_t rstat = rhdr[2];
        sssd_dbg("socket: sss_hdr len=%u cmd=0x%x status=%u", rlen_total, rcmd, rstat);
        if (rlen_total < SSS_NSS_HEADER_SIZE) {
            result->error_code = SSS_SUDO_ERROR_FATAL;
            close(fd);
            drop_after_sssd_query(escalated, saved_euid);
            return result;
        }
        uint32_t status = rstat;
        uint32_t rlen = rlen_total - SSS_NSS_HEADER_SIZE;
        sssd_dbg("socket: status=%u rlen=%u", status, rlen);





        if (status != SSS_SUDO_ERROR_OK) {
            result->error_code = status;
            close(fd);
            drop_after_sssd_query(escalated, saved_euid);
            return result;
        }
        if (rlen == 0 || rlen > (1u<<22)) { /* sanity */
            result->error_code = SSS_SUDO_ERROR_NOENT;
            close(fd);
            drop_after_sssd_query(escalated, saved_euid);
            return result;
            sssd_dbg("socket: will read payload %u bytes", rlen);

        }
        /* Read payload */
        uint8_t *payload = malloc(rlen);
        if (!payload) { goto io_error; }
        if (read_from_socket(fd, payload, rlen) != 0) {
            free(payload);
            goto io_error;
        }
        hex_dump_debug(payload, rlen);

        /* Parse payload TLVs. Collect options/runas before each COMMAND. */
        size_t pos = 0;
        struct sss_rule_ctx ctx; ctx_init_defaults(&ctx);
        while (pos + 8 <= rlen) {
            uint32_t t, l;
            memcpy(&t, payload + pos, 4); pos += 4;
            memcpy(&l, payload + pos, 4); pos += 4;
            t = ntohl(t); l = ntohl(l);
            if (l > rlen - pos) break; /* malformed */
            const uint8_t *val = payload + pos;
            pos += l;

            if (t == (uint32_t)SSS_SUDO_RUNASUSER) {
                size_t cplen = (l < sizeof(ctx.runas_user)-1) ? l : sizeof(ctx.runas_user)-1;
                memcpy(ctx.runas_user, val, cplen); ctx.runas_user[cplen] = '\0';
            } else if (t == (uint32_t)SSS_SUDO_RUNASGROUP) {
                size_t cplen = (l < sizeof(ctx.runas_group)-1) ? l : sizeof(ctx.runas_group)-1;
                memcpy(ctx.runas_group, val, cplen); ctx.runas_group[cplen] = '\0';
            } else if (t == (uint32_t)SSS_SUDO_OPTION) {
                /* tokenise by comma/newline */
                size_t i = 0;
                while (i < l) {
                    char token[256]; size_t tp = 0;
                    while (i < l && (val[i] == ' ' || val[i] == '\n' || val[i] == '\t' || val[i] == ',')) i++;
                    while (i < l && tp < sizeof(token)-1 && val[i] != ',' && val[i] != '\n' && val[i] != '\0') token[tp++] = (char)val[i++];
                    token[tp] = '\0';
                    if (tp > 0) ctx_apply_option(&ctx, token);
                    while (i < l && (val[i] == ',' || val[i] == '\n' || val[i] == '\0')) i++;
                }
            } else if (t == (uint32_t)SSS_SUDO_COMMAND) {
                struct sss_sudo_rule *rule = calloc(1, sizeof(struct sss_sudo_rule));
                if (rule) {
                    copy_ctx_to_rule(&ctx, username, rule);
                    /* Ensure command is NUL-terminated */
                    char *cmdstr = malloc(l + 1);
                    if (cmdstr) {
                        memcpy(cmdstr, val, l); cmdstr[l] = '\0';
                        rule->command = cmdstr;
                    }
                    if (!result->rules) {
                        result->rules = rule;
                    } else {
                        struct sss_sudo_rule *tail = result->rules;
                        while (tail->next) tail = tail->next;
                        tail->next = rule;
                    }
                    result->num_rules++;
                }
                /* reset context minimally between rules? keep cumulative options until next runas/option */
            } else {
                /* ignore other TLVs here */
                (void)val; (void)l;
            }
        }
        free(payload);
        result->error_code = (result->num_rules > 0) ? SSS_SUDO_ERROR_OK : SSS_SUDO_ERROR_NOENT;
        close(fd);
        drop_after_sssd_query(escalated, saved_euid);
        return result;
    }

io_error:
    if (fd >= 0) close(fd);
    drop_after_sssd_query(escalated, saved_euid);
    result->error_code = SSS_SUDO_ERROR_FATAL;
    return result;

    #undef APPEND_DATA
}

/* IPv4 CIDR match: returns 1 if ip_str (dotted) is in cidr (like 192.168.1.0/24). 0 otherwise */
static int ipv4_in_cidr(const char *ip_str, const char *cidr)
{
    if (!ip_str || !cidr) return 0;
    const char *slash = strchr(cidr, '/');
    if (!slash) return 0;
    char net_str[64]; size_t nlen = (size_t)(slash - cidr);
    if (nlen >= sizeof(net_str)) return 0;
    memcpy(net_str, cidr, nlen); net_str[nlen] = '\0';
    int prefix = atoi(slash + 1);
    if (prefix < 0 || prefix > 32) return 0;
    struct in_addr ip, net;
    if (inet_aton(ip_str, &ip) == 0) return 0;
    if (inet_aton(net_str, &net) == 0) return 0;
    uint32_t mask = (prefix == 0) ? 0 : htonl(0xFFFFFFFFu << (32 - prefix));
    return (ip.s_addr & mask) == (net.s_addr & mask);
}

/* Get local host IPv4 addresses (comma-separated into buf). Returns count. */
static int get_local_ipv4_list(char *buf, size_t buflen)
{
    if (!buf || buflen == 0) return 0;
    buf[0] = '\0'; int count = 0; size_t off = 0;
    struct ifaddrs *ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) return 0;
    for (struct ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        char addr[INET_ADDRSTRLEN];
        const char *res = inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, addr, sizeof(addr));
        if (!res) continue;
        size_t len = strlen(addr);
        if (off + len + 1 >= buflen) break;
        if (off > 0) { buf[off++] = ','; }
        memcpy(buf + off, addr, len); off += len; buf[off] = '\0';
        count++;
    }
    freeifaddrs(ifaddr);
    return count;
}

/* Host match: ALL, hostname/FQDN wildcards, IPv4, CIDR, with negation handling */
static int sssd_host_matches(const char *pattern, const char *shortname, const char *fqdn)
{
    if (!pattern) return 0;
    int neg = (pattern[0] == '!');
    const char *pat = neg ? pattern + 1 : pattern;
    int match = 0;
    if (strcmp(pat, "ALL") == 0) match = 1;
    else if (fnmatch(pat, shortname, 0) == 0 || fnmatch(pat, fqdn, 0) == 0) match = 1;
    else {
        /* Try IPv4 and CIDR */
        char local_ips[1024]; get_local_ipv4_list(local_ips, sizeof(local_ips));
        /* iterate CSV */
        const char *p = local_ips; char ip[64]; size_t k = 0;
        for (size_t i = 0; ; ++i) {
            char c = p[i];
            if (c == ',' || c == '\0') {
                ip[k] = '\0';
                if (ip[0]) {
                    if (strcmp(pat, ip) == 0 || ipv4_in_cidr(ip, pat)) { match = 1; }
                }
                k = 0; if (c == '\0') break; else continue;
            } else if (k < sizeof(ip)-1) {
                ip[k++] = c;
            }
        }
    }
    return neg ? !match : match;
}
/* User in group (via gr_mem or primary gid) */
static int sssd_user_in_group(const char *username, const char *groupname)
{
    if (!username || !groupname) return 0;
    struct group *gr = getgrnam(groupname);
    if (!gr) return 0;
    /* check member list */
    if (gr->gr_mem) {
        for (char **m = gr->gr_mem; *m; ++m) {
            if (strcmp(*m, username) == 0) return 1;
        }
    }
    /* check primary gid */
    struct passwd *pw = getpwnam(username);
    if (pw && gr->gr_gid == pw->pw_gid) return 1;
    return 0;
}


/**
 * Free SSSD sudo result structure
 */
static void free_sss_sudo_result(struct sss_sudo_result *result) {
    if (!result) {
        return;
    }

    struct sss_sudo_rule *rule = result->rules;
    while (rule) {
        struct sss_sudo_rule *next = rule->next;

        free(rule->user);
        free(rule->host);
        free(rule->command);
        free(rule->runas_user);
        free(rule->runas_group);

        free(rule->secure_path);
        free(rule->cwd);
        free(rule->chroot_dir);
        free(rule->selinux_role);
        free(rule->selinux_type);
        free(rule->apparmor_profile);
        free(rule->env_keep);
        free(rule->env_check);
        free(rule->env_delete);
        free(rule->iolog_dir);
        free(rule->iolog_file);
        free(rule->iolog_group);

        free(rule);

        rule = next;
    }

    free(result->error_message);
    free(result);
}

/* Simple command matcher: supports EXACT, basename match, and basic globs via fnmatch if available */
static int sssd_command_matches(const char *pattern, const char *command)
{
    if (!pattern || !command) return 0;
    /* Exact or prefix-equal when pattern is ALL */
    if (strcmp(pattern, "ALL") == 0) return 1;
    if (strcmp(pattern, command) == 0) return 1;
    /* Compare basenames */
    const char *pb = strrchr(pattern, '/'); pb = pb ? pb + 1 : pattern;
    const char *cb = strrchr(command, '/'); cb = cb ? cb + 1 : command;
    if (strcmp(pb, cb) == 0) return 1;
#ifdef FNM_PATHNAME
    if (fnmatch(pattern, command, 0) == 0) return 1;
    if (fnmatch(pb, cb, 0) == 0) return 1;
#endif
    return 0;
}



/* User/Host/runas/time/order filters for a rule; hostname inputs must be provided */
static int sssd_rule_applies(const struct sss_sudo_rule *r, const char *username, const char *short_host, const char *fqdn)
{
    if (!r || !username || !short_host || !fqdn) return 0;
    /* time window */
    time_t nowt = time(NULL);
    if (r->not_before && nowt < r->not_before) return 0;
    if (r->not_after && nowt > r->not_after) return 0;
    /* user: support ALL, exact user, and %group */
    if (r->user && strcmp(r->user, "ALL") != 0 && strcmp(r->user, username) != 0) {
        if (r->user[0] == '%') {
            if (!sssd_user_in_group(username, r->user + 1)) return 0;
        } else {
            return 0;
        }
    }
    /* host: if present, must match */
    if (r->host && !sssd_host_matches(r->host, short_host, fqdn)) return 0;
    /* runas filtering: default target is root */
    const char *target_runas = "root";
    if (r->runas_user && r->runas_user[0]) {
        if (strcmp(r->runas_user, "ALL") != 0 && strcmp(r->runas_user, target_runas) != 0) return 0;
    }
    if (r->runas_group && r->runas_group[0]) {
        if (strcmp(r->runas_group, "ALL") != 0) {
            struct passwd *rpw = getpwnam(target_runas);
            if (!rpw) return 0;
            struct group *rg = getgrgid(rpw->pw_gid);
            if (!rg || !rg->gr_name || strcmp(r->runas_group, rg->gr_name) != 0) return 0;
    }
    }
    return 1;
}

/* Stable insertion into a list sorted by sudoOrder (unset=-1 goes last) */
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
        prev = cur; cur = cur->next;
    }
    if (!prev) { nr->next = *head; *head = nr; }
    else { nr->next = cur; prev->next = nr; }
}

/* Check a specific command against SSSD rules (socket/lib query).
 * Conservative precedence: any matching negative (!pattern) denies;
 * otherwise a matching positive or ALL allows; else deny. */
int check_command_permission_sssd(const char *username, const char *command)
{
    if (!username || !command) return 0;
    struct sss_sudo_result *res = query_sssd_sudo_rules(username);
    if (!res || res->error_code != SSS_SUDO_ERROR_OK) { if (res) free_sss_sudo_result(res); return 0; }

    /* Order rules by sudoOrder for consistent precedence */
    struct sss_sudo_rule *sorted = NULL;
    for (struct sss_sudo_rule *r = res->rules; r; ) {
        struct sss_sudo_rule *next = r->next; r->next = NULL; sssd_insert_sorted_by_order(&sorted, r); r = next;
    }
    res->rules = sorted;

    char hostname[256], fqdn[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) snprintf(hostname, sizeof(hostname), "%s", "localhost");
    resolve_fqdn(hostname, fqdn, sizeof(fqdn));

    int any_positive = 0;
    int any_negative = 0;
    for (struct sss_sudo_rule *r = res->rules; r; r = r->next) {
        if (!r->command) continue;
        if (!sssd_rule_applies(r, username, hostname, fqdn)) continue;
        const char *pat = r->command;
        int is_neg = (pat[0] == '!');
        if (is_neg) pat++;
        if (strcmp(pat, "ALL") == 0 && !is_neg) {
            any_positive = 1; continue;
        }
        if (sssd_command_matches(pat, command)) {
            if (is_neg) any_negative = 1; else any_positive = 1;
        }
    }
    free_sss_sudo_result(res);
    if (any_negative) return 0;
    return any_positive ? 1 : 0;
}

/**
 * Check if user has SSSD sudo rules
 */
static int check_sssd_sudo_rules(const char *username) {
    struct sss_sudo_result *result = query_sssd_sudo_rules(username);
    if (!result) {
        return 0;
    }

    int has_rules = (result->num_rules > 0 && result->error_code == SSS_SUDO_ERROR_OK);
    free_sss_sudo_result(result);
    return has_rules;
}

/**
 * Query SSSD for sudo privileges using alternative methods
 */
static int check_sssd_ldap_sudo(const char *username) {
    /* Do not infer sudo privileges from service state or socket presence.
     * Proper authoritative checks require parsing SSSD-provided sudo rules.
     * Returning 0 avoids false positives and closes the bypass tested by security tests. */
    (void)username;
    return 0;
}

/**
 * Get detailed SSSD sudo rules using the SSSD sudo protocol
 */
void get_sssd_sudo_rules_detailed(const char *username, char *output, size_t output_size) {
    if (!username || !output) {
        return;
    }

    output[0] = '\0';  /* Initialize empty string */

    /* Query SSSD for sudo rules */
    struct sss_sudo_result *result = query_sssd_sudo_rules(username);
    if (!result) {
        snprintf(output, output_size, "Failed to connect to SSSD sudo service");
        return;
    }

    if (result->error_code != SSS_SUDO_ERROR_OK) {
        snprintf(output, output_size, "SSSD sudo query failed (error %d)", result->error_code);
        free_sss_sudo_result(result);
        return;
    }

    if (result->num_rules == 0) {
        snprintf(output, output_size, "No SSSD sudo rules found for user");
        free_sss_sudo_result(result);
        return;
    }

    /* Format the rules for display */
    int first_rule = 1;
    struct sss_sudo_rule *rule = result->rules;

    while (rule && strlen(output) < output_size - 100) {
        if (!first_rule) {
            strncat(output, "\n    ", output_size - strlen(output) - 1);
        } else {
            first_rule = 0;
        }

        char rule_str[512];
        snprintf(rule_str, sizeof(rule_str), "(%s) %s  [Source: SSSD/LDAP]",
                 rule->runas_user ? rule->runas_user : "ALL",
                 rule->command ? rule->command : "ALL");

        strncat(output, rule_str, output_size - strlen(output) - 1);
        rule = rule->next;
    }

    free_sss_sudo_result(result);
}

/**
 * Check sudo privileges via SSSD
 */
int check_sssd_privileges(const char *username) {
    if (!username || !is_sssd_available()) {
        return 0;
    }

    /* Try multiple methods to check SSSD sudo privileges */


    /* Method 1: Direct SSSD sudo rules query */
    if (check_sssd_sudo_rules(username)) {
        return 1;
    }

    /* Method 2: LDAP search for sudo rules */
    if (check_sssd_ldap_sudo(username)) {
        return 1;
    }

    /* Method 3: Check if user is in admin groups via SSSD */
    struct group *admin_groups[] = {
        getgrnam("wheel"),
        getgrnam("sudo"),
        getgrnam("admin"),
        NULL
    };

    for (int i = 0; admin_groups[i]; i++) {
        struct group *grp = admin_groups[i];
        if (grp && grp->gr_mem) {
            for (char **member = grp->gr_mem; *member; member++) {
                if (strcmp(*member, username) == 0) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/**
 * Initialize SSSD connection (placeholder)
 */
int init_sssd_connection(void) {
    if (!is_sssd_available()) {
        return 0;
    }

    /* In a full implementation, this would:
     * 1. Connect to SSSD D-Bus interface
     * 2. Initialize SSSD client libraries
     * 3. Set up authentication context
     */

    return 1;
}

/**
 * Cleanup SSSD connection (placeholder)
 */
void cleanup_sssd_connection(void) {
    /* Cleanup SSSD resources */
}

/**
 * Get hostname for SSSD queries
 */
static char *get_hostname(void) {
    static char hostname[256];

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return hostname;
    }

    return "localhost";
}

/**
 * Check if user has sudo privileges via SSSD with hostname
 */
int check_sssd_privileges_with_host(const char *username, const char *hostname) {
    if (!username) {
        return 0;
    }

    if (!hostname) {
        hostname = get_hostname();
    }

    /* For now, delegate to the simpler check */
    return check_sssd_privileges(username);
}
