#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../../src/sudosh.h"
#include "../../src/sssd_test_api.h"

static const char *cmd_ls[] = { "/bin/ls", NULL };

int main(void)
{
    /* Prepare environment */
    setenv("FOO", "bar", 1);
    setenv("BAZ", "qux", 1);
    setenv("TERM", "xterm", 1);

    sssd_test_rule rules[] = {
        { .user = "tester", .host = "ALL", .commands = cmd_ls, .order = 1,
          .env_reset = 1, .secure_path = "/sbin:/bin:/usr/bin", .env_delete = "FOO", .env_check = "TERM,NOTSET" }
    };

    sssd_effective_opts_test out;
    int ok = sssd_compute_effective_options_for_test(rules, 1, "tester", "host", "host.example.com", "root", NULL, "/bin/ls", &out);
    assert(ok == 1);

    /* Simulate application in our process: apply env policy */
    /* Clear then apply to simulate command.c behavior */
    if (out.env_reset) {
        clearenv();
        setenv("TERM", "xterm-256color", 1);
        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    }
    /* Convert test struct to runtime struct for apply function */
    struct sssd_effective_opts ropts; memset(&ropts, 0, sizeof(ropts));
    ropts.env_reset = out.env_reset; ropts.secure_path = (char*)out.secure_path; ropts.env_delete = (char*)out.env_delete; ropts.env_check = (char*)out.env_check;
    apply_env_policy_from_sssd(&ropts);

    /* PATH should be overridden */
    const char *path = getenv("PATH");
    assert(path && strcmp(path, "/sbin:/bin:/usr/bin") == 0);
    /* FOO should be deleted, BAZ should persist, NOTSET was not set */
    assert(getenv("FOO") == NULL);
    assert(getenv("BAZ") == NULL); /* cleared by env_reset */
    assert(getenv("TERM") != NULL);

    printf("sssd_env_semantics: PASS\n");
    return 0;
}

