#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "sudosh.h"

/* These tests exercise the public API check_command_permission_sssd_as().
 * They are smoke-level because we cannot easily stand up a live SSSD; we
 * rely on the existing pipeline regression and payload parser tests for
 * end-to-end coverage. Here we only call the function signature to ensure
 * linkage and default behaviors do not crash.
 */

int main(void)
{
    /* Just verify the symbol links and returns a boolean (likely 0 without SSSD). */
    int r1 = check_command_permission_sssd_as("dummyuser", "/bin/true", "root", NULL);
    int r2 = check_command_permission_sssd("dummyuser", "/bin/true");
    printf("check_command_permission_sssd_as returned %d, default returned %d\n", r1, r2);
    /* Not asserting true/false to keep this portable; ensure function is callable. */
    return 0;
}

