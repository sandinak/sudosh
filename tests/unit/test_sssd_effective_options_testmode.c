#include <stdio.h>
#include <assert.h>
#include "sssd_test_api.h"

static const char *cmd_all[] = { "ALL", NULL };
static const char *cmd_neg_sh[] = { "!/bin/sh", NULL };

/* In test mode we only assert allow/deny wiring and struct defaults are sane */
int main(void)
{
    sssd_test_rule rules[] = {
        { .user = "tester", .host = "ALL", .runas_user = "ALL", .commands = cmd_all, .order = 10 },
        { .user = "tester", .host = "ALL", .runas_user = "ALL", .commands = cmd_neg_sh, .order = 20 }
    };

    int allow = sssd_eval_rules_for_test(rules, 2, "tester", "host", "host.example.com", "root", NULL, "/bin/ls");
    assert(allow == 1);
    allow = sssd_eval_rules_for_test(rules, 2, "tester", "host", "host.example.com", "root", NULL, "/bin/sh");
    assert(allow == 0);

    sssd_effective_opts_test out;
    int ok = sssd_compute_effective_options_for_test(rules, 2, "tester", "host", "host.example.com", "root", NULL, "/bin/ls", &out);
    assert(ok == 1);
    /* Defaults are meaningful */
    assert(out.umask_value == -1);
    assert(out.timestamp_timeout == -1);
    assert(out.env_reset == 0 && out.noexec == 0);

    printf("sssd_effective_options testmode: PASS\n");
    return 0;
}

