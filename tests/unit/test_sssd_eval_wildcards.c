#include <stdio.h>
#include <assert.h>
#include "sssd_test_api.h"

static const char *cmdall[] = { "ALL", NULL };

static void test_host_wildcard_domain(void)
{
    sssd_test_rule rules[] = {
        { .user = "user1", .host = "*.example.com", .runas_user = "ALL", .commands = cmdall, .order = 1 }
    };
    int allow = sssd_eval_rules_for_test(rules, 1, "user1", "web01", "web01.example.com", "root", NULL, "/bin/true");
    assert(allow == 1);
}

static void test_host_wildcard_shortname(void)
{
    sssd_test_rule rules[] = {
        { .user = "user1", .host = "web??", .runas_user = "ALL", .commands = cmdall, .order = 1 }
    };
    int allow = sssd_eval_rules_for_test(rules, 1, "user1", "web01", "web01.example.com", "root", NULL, "/bin/true");
    assert(allow == 1);
}

static void test_host_ipv4_direct_and_cidr(void)
{
    sssd_test_rule rules_eq[] = {
        { .user = "user1", .host = "192.168.56.101", .runas_user = "ALL", .commands = cmdall, .order = 1 }
    };
    sssd_test_rule rules_cidr[] = {
        { .user = "user1", .host = "192.168.56.0/24", .runas_user = "ALL", .commands = cmdall, .order = 1 }
    };
    int allow = sssd_eval_rules_for_test(rules_eq, 1, "user1", "192.168.56.101", "192.168.56.101", "root", NULL, "/bin/true");
    assert(allow == 1);
    allow = sssd_eval_rules_for_test(rules_cidr, 1, "user1", "192.168.56.101", "192.168.56.101", "root", NULL, "/bin/true");
    assert(allow == 1);
}

int main(void)
{
    test_host_wildcard_domain();
    test_host_wildcard_shortname();
    test_host_ipv4_direct_and_cidr();
    printf("sssd_eval wildcard tests passed.\n");
    return 0;
}

