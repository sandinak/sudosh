#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test dangerous alias pattern detection */
int test_check_dangerous_alias_patterns() {
    /* Test critical command override attempts */
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("sudo", "ls -la"), 
                   "Aliasing sudo should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("passwd", "echo safe"), 
                   "Aliasing passwd should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("su", "whoami"), 
                   "Aliasing su should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("chmod", "ls"), 
                   "Aliasing chmod should be dangerous");
    
    /* Test aliases containing privileged commands */
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("myalias", "sudo ls"), 
                   "Alias containing sudo should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("backup", "rsync --delete"), 
                   "Alias containing rsync should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("restart", "systemctl restart"), 
                   "Alias containing systemctl should be dangerous");
    
    /* Test environment manipulation */
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("setpath", "PATH=/tmp:$PATH"), 
                   "Alias modifying PATH should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("preload", "LD_PRELOAD=/tmp/lib.so"), 
                   "Alias setting LD_PRELOAD should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("exppath", "export PATH=/tmp"), 
                   "Alias exporting PATH should be dangerous");
    
    /* Test execution from dangerous locations */
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("tmpexec", "/tmp/malicious"), 
                   "Alias executing from /tmp should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("hidden", "~/.hidden_script"), 
                   "Alias executing hidden files should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("shmexec", "/dev/shm/script"), 
                   "Alias executing from /dev/shm should be dangerous");
    
    /* Test safe aliases */
    TEST_ASSERT_EQ(0, check_dangerous_alias_patterns("ll", "ls -la"), 
                   "Safe ls alias should not be dangerous");
    TEST_ASSERT_EQ(0, check_dangerous_alias_patterns("grep", "grep --color=auto"), 
                   "Safe grep alias should not be dangerous");
    TEST_ASSERT_EQ(0, check_dangerous_alias_patterns("myls", "ls -lah"), 
                   "Safe custom ls alias should not be dangerous");
    
    return 1;
}

/* Test alias expansion safety validation */
int test_validate_alias_expansion_safety() {
    /* Initialize alias system */
    init_alias_system();
    
    /* Test safe alias expansion */
    TEST_ASSERT_EQ(1, validate_alias_expansion_safety("ll", "ls -la"), 
                   "Safe alias expansion should be valid");
    TEST_ASSERT_EQ(1, validate_alias_expansion_safety("mygrep", "grep --color=auto"), 
                   "Safe grep alias should be valid");
    
    /* Test self-referential alias */
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety("ls", "ls"), 
                   "Self-referential alias should be invalid");
    
    /* Test recursive alias */
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety("test", "test -f"), 
                   "Recursive alias should be invalid");
    
    /* Test alias with dangerous expansion */
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety("danger", "rm -rf /"), 
                   "Dangerous expansion should be invalid");
    
    /* Test alias with shell metacharacters */
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety("shell", "ls; rm file"), 
                   "Alias with shell metacharacters should be invalid");
    
    return 1;
}

/* Test expanded alias command validation */
int test_validate_expanded_alias_command() {
    /* Test safe expanded command */
    TEST_ASSERT_EQ(1, validate_expanded_alias_command("ls -la", "ll", "ls -la"), 
                   "Safe expanded command should be valid");
    
    /* Test dangerous expanded command */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("rm -rf /", "danger", "rm -rf /"), 
                   "Dangerous expanded command should be invalid");
    
    /* Test self-referential expansion */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls -la", "ls", "ls"), 
                   "Self-referential expansion should be invalid");
    
    /* Test expansion with shell metacharacters */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls; rm file", "bad", "ls; rm file"), 
                   "Expansion with shell metacharacters should be invalid");
    
    /* Test expansion with command substitution */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls `whoami`", "sub", "ls `whoami`"), 
                   "Expansion with command substitution should be invalid");
    
    /* Test expansion with redirection */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls > /etc/passwd", "redir", "ls > /etc/passwd"), 
                   "Expansion with unsafe redirection should be invalid");
    
    /* Test expansion with safe redirection */
    TEST_ASSERT_EQ(1, validate_expanded_alias_command("ls > /tmp/output.txt", "safe_redir", "ls > /tmp/output.txt"), 
                   "Expansion with safe redirection should be valid");
    
    /* Test NULL inputs */
    TEST_ASSERT_EQ(0, validate_expanded_alias_command(NULL, "alias", "value"), 
                   "NULL expanded command should be invalid");
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls", NULL, "value"), 
                   "NULL alias name should be invalid");
    TEST_ASSERT_EQ(0, validate_expanded_alias_command("ls", "alias", NULL), 
                   "NULL alias value should be invalid");
    
    return 1;
}

/* Test alias expansion with security validation */
int test_expand_aliases_security() {
    /* Initialize alias system */
    init_alias_system();
    
    /* Add a safe alias */
    TEST_ASSERT_EQ(1, add_alias("ll", "ls -la"), "Should be able to add safe alias");
    
    /* Test safe alias expansion */
    char *expanded = expand_aliases("ll");
    TEST_ASSERT_NOT_NULL(expanded, "Safe alias should expand");
    if (expanded) {
        TEST_ASSERT_STR_EQ("ls -la", expanded, "Alias should expand correctly");
        free(expanded);
    }
    
    /* Test expansion with arguments */
    expanded = expand_aliases("ll /tmp");
    TEST_ASSERT_NOT_NULL(expanded, "Alias with args should expand");
    if (expanded) {
        TEST_ASSERT_STR_EQ("ls -la /tmp", expanded, "Alias with args should expand correctly");
        free(expanded);
    }
    
    /* Test non-alias command */
    expanded = expand_aliases("pwd");
    TEST_ASSERT_NOT_NULL(expanded, "Non-alias command should return original");
    if (expanded) {
        TEST_ASSERT_STR_EQ("pwd", expanded, "Non-alias should return unchanged");
        free(expanded);
    }
    
    /* Try to add a dangerous alias (should fail) */
    TEST_ASSERT_EQ(0, add_alias("danger", "rm -rf /"), "Should not be able to add dangerous alias");
    
    /* Test that dangerous alias doesn't expand */
    expanded = expand_aliases("danger");
    TEST_ASSERT_NOT_NULL(expanded, "Unknown alias should return original");
    if (expanded) {
        TEST_ASSERT_STR_EQ("danger", expanded, "Unknown alias should return unchanged");
        free(expanded);
    }
    
    return 1;
}

/* Test internal alias expansion without security validation */
int test_expand_aliases_internal() {
    /* Initialize alias system */
    init_alias_system();
    
    /* Add a safe alias */
    add_alias("test_internal", "ls -la");
    
    /* Test internal expansion */
    char *expanded = expand_aliases_internal("test_internal");
    TEST_ASSERT_NOT_NULL(expanded, "Internal expansion should work");
    if (expanded) {
        TEST_ASSERT_STR_EQ("ls -la", expanded, "Internal expansion should be correct");
        free(expanded);
    }
    
    /* Test internal expansion with arguments */
    expanded = expand_aliases_internal("test_internal /tmp");
    TEST_ASSERT_NOT_NULL(expanded, "Internal expansion with args should work");
    if (expanded) {
        TEST_ASSERT_STR_EQ("ls -la /tmp", expanded, "Internal expansion with args should be correct");
        free(expanded);
    }
    
    /* Test non-alias */
    expanded = expand_aliases_internal("nonexistent");
    TEST_ASSERT_NOT_NULL(expanded, "Non-alias should return original");
    if (expanded) {
        TEST_ASSERT_STR_EQ("nonexistent", expanded, "Non-alias should be unchanged");
        free(expanded);
    }
    
    /* Test NULL input */
    expanded = expand_aliases_internal(NULL);
    TEST_ASSERT_NULL(expanded, "NULL input should return NULL");
    
    return 1;
}

/* Test alias validation integration */
int test_alias_validation_integration() {
    /* Initialize alias system */
    init_alias_system();
    
    /* Test that add_alias uses all validation layers */
    
    /* Should reject dangerous patterns */
    TEST_ASSERT_EQ(0, add_alias("sudo", "ls"), "Should reject critical command override");
    TEST_ASSERT_EQ(0, add_alias("bad", "sudo ls"), "Should reject alias containing sudo");
    TEST_ASSERT_EQ(0, add_alias("env", "PATH=/tmp"), "Should reject environment manipulation");
    
    /* Should reject dangerous commands */
    TEST_ASSERT_EQ(0, add_alias("rm_all", "rm -rf /"), "Should reject dangerous command");
    TEST_ASSERT_EQ(0, add_alias("shell", "bash"), "Should reject shell command");
    
    /* Should reject shell metacharacters */
    TEST_ASSERT_EQ(0, add_alias("chain", "ls; rm file"), "Should reject command chaining");
    TEST_ASSERT_EQ(0, add_alias("sub", "ls `whoami`"), "Should reject command substitution");
    
    /* Should accept safe aliases */
    TEST_ASSERT_EQ(1, add_alias("ll", "ls -la"), "Should accept safe alias");
    TEST_ASSERT_EQ(1, add_alias("mygrep", "grep --color=auto"), "Should accept safe grep alias");
    
    return 1;
}

/* Test edge cases and error handling */
int test_alias_validation_edge_cases() {
    /* Test NULL inputs */
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns(NULL, "value"),
                   "NULL alias name should be dangerous");
    TEST_ASSERT_EQ(1, check_dangerous_alias_patterns("name", NULL),
                   "NULL alias value should be dangerous");
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety(NULL, "value"), 
                   "NULL alias name should be invalid");
    TEST_ASSERT_EQ(0, validate_alias_expansion_safety("name", NULL), 
                   "NULL alias value should be invalid");
    
    /* Test empty strings */
    TEST_ASSERT_EQ(0, check_dangerous_alias_patterns("", "value"), 
                   "Empty alias name should be dangerous");
    TEST_ASSERT_EQ(0, check_dangerous_alias_patterns("name", ""), 
                   "Empty alias value should be dangerous");
    
    /* Test very long inputs */
    char long_name[MAX_ALIAS_NAME_LENGTH + 10];
    char long_value[MAX_ALIAS_VALUE_LENGTH + 10];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    memset(long_value, 'b', sizeof(long_value) - 1);
    long_value[sizeof(long_value) - 1] = '\0';
    
    TEST_ASSERT_EQ(0, add_alias(long_name, "ls"), "Should reject overly long alias name");
    TEST_ASSERT_EQ(0, add_alias("test", long_value), "Should reject overly long alias value");
    
    return 1;
}

TEST_SUITE_BEGIN("Alias Validation Tests")
    RUN_TEST(test_check_dangerous_alias_patterns);
    RUN_TEST(test_validate_alias_expansion_safety);
    RUN_TEST(test_validate_expanded_alias_command);
    RUN_TEST(test_expand_aliases_security);
    RUN_TEST(test_expand_aliases_internal);
    RUN_TEST(test_alias_validation_integration);
    RUN_TEST(test_alias_validation_edge_cases);
TEST_SUITE_END()
