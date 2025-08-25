#include "sudosh.h"
#include <stdio.h>

int main(void) {
    const char *special_commands[] = {
        "ls && whoami",
        "ls || whoami",
        "ls & whoami",
        "ls; whoami",
        "ls | whoami",
        "ls `whoami`",
        "ls $(whoami)",
        "ls ${USER}",
        "ls $USER",
        "ls 'test'",
        "ls \"test\"",
        "ls \\test",
        NULL
    };
    for (int i = 0; special_commands[i]; i++) {
        int r = validate_command(special_commands[i]);
        printf("%d %s\n", r, special_commands[i]);
    }
    return 0;
}

