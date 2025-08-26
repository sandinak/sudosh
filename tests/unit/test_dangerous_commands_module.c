#include "../test_framework.h"
#include "../../src/dangerous_commands.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_critical_dangerous_detection() {
    TEST_ASSERT(is_critical_dangerous_command("/bin/rm -rf /") == 1, "rm is critical");
    TEST_ASSERT(is_critical_dangerous_command("shutdown now") == 1, "shutdown critical");
    TEST_ASSERT(is_critical_dangerous_command("echo hello") == 0, "echo not critical");
    return 1;
}

static int test_moderate_dangerous_detection() {
    TEST_ASSERT(is_moderate_dangerous_command("vi /etc/passwd") == 1, "vi moderate");
    TEST_ASSERT(is_moderate_dangerous_command("nano file.txt") == 1, "nano moderate");
    TEST_ASSERT(is_moderate_dangerous_command("cat file.txt") == 1, "cat moderate set");
    return 1;
}

static int test_sensitive_paths_and_patterns() {
    TEST_ASSERT(involves_sensitive_paths("echo test > /etc/hosts") == 1, "sensitive path");
    TEST_ASSERT(contains_dangerous_patterns("rm -rf /tmp") == 1, "dangerous flag pattern");
    TEST_ASSERT(contains_dangerous_patterns("ls -la") == 0, "no dangerous pattern");
    return 1;
}

static int test_requires_password_in_editor_and_explanation() {
    TEST_ASSERT(requires_password_in_editor("rm -rf /") == 1, "critical requires password");
    TEST_ASSERT(requires_password_in_editor("vi /etc/passwd") == 1, "moderate requires password");
    TEST_ASSERT(requires_password_in_editor("echo hello") == 0, "safe command doesn't");

    const char *ex1 = get_danger_explanation("rm");
    TEST_ASSERT(ex1 != NULL, "explanation string");
    const char *ex2 = get_danger_explanation("echo");
    TEST_ASSERT(ex2 != NULL, "explanation safe");
    return 1;
}

TEST_SUITE_BEGIN("Dangerous Commands Unit Tests")
    RUN_TEST(test_critical_dangerous_detection);
    RUN_TEST(test_moderate_dangerous_detection);
    RUN_TEST(test_sensitive_paths_and_patterns);
    RUN_TEST(test_requires_password_in_editor_and_explanation);
TEST_SUITE_END()

