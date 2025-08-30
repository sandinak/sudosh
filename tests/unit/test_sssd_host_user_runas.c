#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "sudosh.h"

/* Targeted unit tests for API-level behavior without asserting allow/deny
 * since live SSSD is not present. These are link/invocation tests.
 */

static void call_variants(void)
{
    (void)check_command_permission_sssd("user1", "/bin/true");
    (void)check_command_permission_sssd_as("user1", "/bin/true", "root", NULL);
    (void)check_command_permission_sssd_as("user1", "/usr/bin/id", "root", "root");
    (void)check_command_permission_sssd_as("user1", "/usr/bin/id", "nobody", NULL);
}

int main(void)
{
    call_variants();
    printf("API invocation tests executed.\n");
    return 0;
}

