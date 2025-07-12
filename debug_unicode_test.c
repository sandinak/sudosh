#include "src/sudosh.h"

int main() {
    const char *unicode_commands[] = {
        "ls\x00whoami",           /* Null byte */
        "ls\x0awhoami",           /* Newline */
        "ls\x0dwhoami",           /* Carriage return */
        "ls\x09whoami",           /* Tab */
        "ls%00whoami",            /* URL encoded null */
        "ls%0awhoami",            /* URL encoded newline */
        "ls\\u0000whoami",        /* Unicode null */
        NULL
    };
    
    for (int i = 0; unicode_commands[i]; i++) {
        printf("Testing command %d: ", i);
        for (int j = 0; unicode_commands[i][j]; j++) {
            if (unicode_commands[i][j] >= 32 && unicode_commands[i][j] <= 126) {
                printf("%c", unicode_commands[i][j]);
            } else {
                printf("\\x%02x", (unsigned char)unicode_commands[i][j]);
            }
        }
        printf("\n");
        
        int result = validate_command(unicode_commands[i]);
        printf("Result: %s\n\n", result ? "ALLOWED (vulnerable)" : "BLOCKED (secure)");
    }
    
    return 0;
}
