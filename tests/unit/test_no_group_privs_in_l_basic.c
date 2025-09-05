#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "sudosh.h"

/* Smoke test: -l (basic) no longer prints group-based privileges text */
int main(void)
{
    char *username = get_current_username();
    if (!username) return 0;

    FILE *temp = tmpfile();
    if (!temp) return 0;
    FILE *orig = stdout;
    stdout = temp;
    list_available_commands_basic(username);
    stdout = orig;
    rewind(temp);
    char buf[512]; int ok = 1;
    while (fgets(buf, sizeof(buf), temp)) {
        if (strstr(buf, "Group-Based Privileges")) { ok = 0; break; }
    }
    fclose(temp);
    printf("basic -l group-privs hidden: %s\n", ok ? "PASS" : "FAIL");
    return 0;
}

