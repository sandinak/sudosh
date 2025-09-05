#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/sudosh.h"
#include "../../src/sssd_test_api.h"

static const char *cmd_ls[] = { "/bin/ls", NULL };

int main(void)
{
    setenv("TERM", "xterm", 1);
    setenv("DANG", "1", 1);

    sssd_test_rule rules[] = {
        { .user = "tester", .host = "ALL", .commands = cmd_ls, .order = 1,
          .env_reset = 1, .env_check = "TERM,NOTSET", .secure_path = "/usr/bin:/bin" }
    };

    sssd_effective_opts_test out;
    int ok = sssd_compute_effective_options_for_test(rules, 1, "tester", "h", "h.example.com", "root", NULL, "/bin/ls", &out);
    assert(ok == 1);

    struct sssd_effective_opts ropts; memset(&ropts, 0, sizeof(ropts));
    ropts.env_reset = out.env_reset; ropts.env_check = (char*)out.env_check; ropts.secure_path = (char*)out.secure_path;

    apply_env_reset_and_policy_from_sssd(&ropts);

    /* TERM should persist from baseline; NOTSET removed (if existed); PATH overridden */
    const char *p = getenv("PATH");
    assert(p && strcmp(p, "/usr/bin:/bin") == 0);
    assert(getenv("TERM") != NULL);

    printf("sssd_env_check_validation: PASS\n");
    return 0;
}

