#include <stdio.h>
#include <assert.h>
#include "sssd_test_api.h"

static const char *cmd_all[] = { "ALL", NULL };
static const char *cmd_ls[] = { "/bin/ls", NULL };

int main(void)
{
    sssd_test_rule rules[] = {
        { .user = "tester", .host = "ALL", .runas_user = "ALL", .commands = cmd_all, .order = 5,
          .env_reset = 1, .secure_path = "/sbin:/bin", .umask_value = 077 },
        { .user = "tester", .host = "ALL", .runas_user = "ALL", .commands = cmd_ls, .order = 10,
          .noexec = 1, .timestamp_timeout = 5 }
    };

    sssd_effective_opts_test out;
    int ok = sssd_compute_effective_options_for_test(rules, 2, "tester", "host", "host.example.com", "root", NULL, "/bin/ls", &out);
    assert(ok == 1);
    assert(out.env_reset == 1);
    assert(out.noexec == 1);
    assert(out.secure_path && out.secure_path[0] == '/');
    assert(out.umask_value == 077);
    assert(out.timestamp_timeout == 5);
    printf("sssd_effective_options union: PASS\n");
    return 0;
}

