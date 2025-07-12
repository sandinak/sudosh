#include "src/sudosh.h"

int main() {
    const char *malicious_users[] = {
        "",                   /* Empty username */
        "user;rm -rf /",     /* Semicolon injection */
        "user`whoami`",      /* Backtick injection */
        "user$(id)",         /* Command substitution */
        "user/../root",      /* Path traversal */
        "user|cat",          /* Pipe injection */
        "user&whoami",       /* Background command */
        "user>file",         /* Redirection */
        "user<file",         /* Input redirection */
        "user*",             /* Wildcard */
        "user?",             /* Wildcard */
        "user~",             /* Tilde expansion */
        "user{a,b}",         /* Brace expansion */
        "user[abc]",         /* Character class */
        "user\\escape",      /* Backslash escape */
        "user'quote'",       /* Single quote */
        "user\"quote\"",     /* Double quote */
        "user\x00null",      /* Null byte */
        "user\nnewline",     /* Newline */
        "user\ttab",         /* Tab */
        "root",              /* Suspicious username */
        "admin",             /* Suspicious username */
        "administrator",     /* Suspicious username */
        "test",              /* Suspicious username */
        NULL
    };
    
    for (int i = 0; malicious_users[i]; i++) {
        printf("Testing malicious user %d: '", i);
        for (int j = 0; malicious_users[i][j]; j++) {
            char c = malicious_users[i][j];
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            } else {
                printf("\\x%02x", (unsigned char)c);
            }
        }
        printf("'\n");
        
        if (authenticate_user(malicious_users[i])) {
            printf("  VULNERABLE: Authentication succeeded!\n");
            return 1;
        } else {
            printf("  SECURE: Authentication failed\n");
        }
    }
    
    printf("\nAll malicious usernames were rejected - SECURE\n");
    return 0;
}
