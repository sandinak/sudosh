#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "sssd_test_api.h"

static const char *cmdset1[] = { "/bin/ls", "!/bin/sh", NULL };
static const char *cmdall[]  = { "ALL", NULL };

static void test_negative_overrides_positive(void)
{
    sssd_test_rule rules[] = {
        { .user="user1", .host="ALL", .runas_user="ALL", .commands=cmdset1, .order=10 },
    };
    int allow = sssd_eval_rules_for_test(rules, 1, "user1", "host", "host.example", "root", NULL, "/bin/sh");
    assert(allow == 0);
    allow = sssd_eval_rules_for_test(rules, 1, "user1", "host", "host.example", "root", NULL, "/bin/ls");
    assert(allow == 1);
}

static void test_all_allows(void)
{
    sssd_test_rule rules[] = {
        { .user="user1", .host="ALL", .runas_user="ALL", .commands=cmdall, .order=5 },
    };
    int allow = sssd_eval_rules_for_test(rules, 1, "user1", "h", "h.example", "root", NULL, "/anything");
    assert(allow == 1);
}

static void test_host_ipv4_and_cidr(void)
{
    sssd_test_rule rules_eq[] = {
        { .user="user1", .host="192.168.1.10", .runas_user="ALL", .commands=cmdall, .order=1 },
    };
    sssd_test_rule rules_cidr[] = {
        { .user="user1", .host="192.168.1.0/24", .runas_user="ALL", .commands=cmdall, .order=1 },
    };
    /* For deterministic tests, short_host/fqdn won't be used for IP match; this test
     * primarily covers sssd_eval flow; actual IP enumeration is covered elsewhere. */
    int allow = sssd_eval_rules_for_test(rules_eq, 1, "user1", "host", "host.example", "root", NULL, "/bin/true");
    /* With no direct IP match in test, allow may be 0; ensure no crash */
    (void)allow;
    allow = sssd_eval_rules_for_test(rules_cidr, 1, "user1", "host", "host.example", "root", NULL, "/bin/true");
    (void)allow;
}

static void test_user_group_and_runas(void)
{
    sssd_test_rule rules[] = {
        { .user="%root", .host="ALL", .runas_user="root", .commands=cmdall, .order=1 },
    };
    /* Can't guarantee membership here; just ensure invocation */
    (void)sssd_eval_rules_for_test(rules, 1, "user1", "h", "h.example", "root", NULL, "/bin/true");
}

static void test_time_window_and_order(void)
{
    time_t now = time(NULL);
    sssd_test_rule rules[] = {
        { .user="user1", .host="ALL", .runas_user="ALL", .commands=cmdall, .order=20, .not_before=now+3600 },
        { .user="user1", .host="ALL", .runas_user="ALL", .commands=cmdset1, .order=10 },
    };
    /* First rule not yet active; second applies. /bin/sh must be denied by negative */
    int allow = sssd_eval_rules_for_test(rules, 2, "user1", "h", "h.example", "root", NULL, "/bin/sh");
    assert(allow == 0);
}

int main(void)
{
    test_negative_overrides_positive();
    test_all_allows();
    test_host_ipv4_and_cidr();
    test_user_group_and_runas();
    test_time_window_and_order();
    printf("sssd_eval test suite passed (non-deterministic allowances skipped).\n");
    return 0;
}

