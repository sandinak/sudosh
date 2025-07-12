#include "src/sudosh.h"

int main() {
    const char *test_users[] = {
        "",                    /* Empty username */
        "user;rm",            /* Semicolon */
        "user`whoami`",       /* Backtick */
        "user$(id)",          /* Dollar sign */
        "user/../root",       /* Slash */
        "validuser",          /* Valid username */
        NULL
    };
    
    for (int i = 0; test_users[i]; i++) {
        printf("Testing username: '%s'\n", test_users[i]);
        int result = authenticate_user(test_users[i]);
        printf("Result: %s\n\n", result ? "ACCEPTED (vulnerable)" : "REJECTED (secure)");
    }
    
    return 0;
}
