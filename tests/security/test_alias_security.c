#include "test_framework.h"
#include "sudosh.h"
#include <string.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static void reset_alias(const char *name) {
    remove_alias(name);
}

int test_alias_name_validation() {
    printf("Running test_alias_name_validation... ");
    init_alias_system();

    /* Invalid: starts with digit */
    TEST_ASSERT_EQ(0, add_alias("1bad", "echo ok"), "alias names must not start with digit");

    /* Invalid: builtin override */
    TEST_ASSERT_EQ(0, add_alias("cd", "echo nope"), "should not allow overriding builtin");

    /* Valid: underscores and alnum */
    TEST_ASSERT_EQ(1, add_alias("good_name1", "echo hi"), "valid alias should be added");
    reset_alias("good_name1");

    printf("PASS\n");
    return 1;
}

int test_alias_value_validation_blocks_shells_and_ops() {
    printf("Running test_alias_value_validation_blocks_shells_and_ops... ");
    init_alias_system();

    /* Block shells */
    TEST_ASSERT_EQ(0, add_alias("sh1", "sh"), "shell command should be rejected");
    TEST_ASSERT_EQ(0, add_alias("bash1", "bash -c ls"), "bash -c should be rejected");

    /* Block sudoedit and ssh */
    TEST_ASSERT_EQ(0, add_alias("sudoedit1", "sudoedit /etc/passwd"), "sudoedit should be rejected");
    TEST_ASSERT_EQ(0, add_alias("ssh1", "ssh root@host"), "ssh should be rejected");

    /* Block pipes and redirection */
    TEST_ASSERT_EQ(0, add_alias("pipe1", "echo hi | rm -rf /"), "pipes should be rejected");
    TEST_ASSERT_EQ(0, add_alias("redir1", "echo hi > /tmp/x"), "> should be rejected");

    /* Block command substitution */
    TEST_ASSERT_EQ(0, add_alias("subst1", "echo $(id)"), "command substitution should be rejected");

    printf("PASS\n");
    return 1;
}

int test_alias_expansion_and_runtime_validation_safe() {
    printf("Running test_alias_expansion_and_runtime_validation_safe... ");
    init_alias_system();

    /* Safe alias to a read-only command */
    TEST_ASSERT_EQ(1, add_alias("ll", "ls -la"), "safe alias should be added");

    char *expanded = expand_aliases("ll /usr");
    TEST_ASSERT_NOT_NULL(expanded, "expanded should not be NULL");
    /* Expanded should begin with ls */
    TEST_ASSERT(strncmp(expanded, "ls ", 3) == 0 || strncmp(expanded, "ls-", 3) == 0 || strncmp(expanded, "ls", 2) == 0, "expanded should begin with ls");

    /* The final expanded command must pass validation */
    TEST_ASSERT_EQ(1, validate_command(expanded), "validate_command should allow safe expanded alias");

    free(expanded);
    reset_alias("ll");
    printf("PASS\n");
    return 1;
}

int test_alias_expansion_blocks_dangerous_at_runtime() {
    printf("Running test_alias_expansion_blocks_dangerous_at_runtime... ");
    init_alias_system();

    /* Attempt to add a borderline alias; import layer should already block */
    int added = add_alias("rmr", "rm -rf");
    /* Expected path: not added due to validation */
    TEST_ASSERT_EQ(0, added, "dangerous alias should be rejected at add time");

    printf("PASS\n");
    return 1;
}

int test_alias_length_limits() {
    printf("Running test_alias_length_limits... ");
    init_alias_system();

    /* Over-long name */
    char longname[128];
    memset(longname, 'a', sizeof(longname));
    longname[sizeof(longname)-1] = '\0';
    TEST_ASSERT_EQ(0, add_alias(longname, "echo ok"), "overlong alias name should be rejected");

    /* Over-long value */
    char longval[2048];
    memset(longval, 'b', sizeof(longval));
    longval[sizeof(longval)-1] = '\0';
    TEST_ASSERT_EQ(0, add_alias("okname", longval), "overlong alias value should be rejected");
    reset_alias("okname");

    printf("PASS\n");
    return 1;
}

int main(void) {
    test_passes += test_alias_name_validation();
    test_passes += test_alias_value_validation_blocks_shells_and_ops();
    test_passes += test_alias_expansion_and_runtime_validation_safe();
    test_passes += test_alias_expansion_blocks_dangerous_at_runtime();
    test_passes += test_alias_length_limits();

    test_count = 5;
    test_failures = test_count - test_passes;

    printf("\n=== Alias Security Tests ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);

    return (test_failures == 0) ? 0 : 1;
}

