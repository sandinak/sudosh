#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_expand_equals_expression() {
    char *out;

    out = expand_equals_expression("=ls");
    TEST_ASSERT_NOT_NULL(out, "=ls should expand");
    free(out);

    out = expand_equals_expression("ls");
    TEST_ASSERT_STR_EQ("ls", out, "non = expr returns copy");
    free(out);

    out = expand_equals_expression("=");
    TEST_ASSERT_NOT_NULL(out, "just '=' should return copy");
    free(out);

    return 1;
}

static int test_parse_command_basic() {
    struct command_info cmd;
    memset(&cmd, 0, sizeof(cmd));

    TEST_ASSERT_EQ(0, parse_command("echo hello", &cmd), "parse simple command");
    TEST_ASSERT_EQ(2, cmd.argc, "argc should be 2");
    TEST_ASSERT_STR_EQ("echo hello", cmd.command, "command string preserved");

    free_command_info(&cmd);
    return 1;
}

static int test_parse_command_with_redirection_parsed_via_shell_ops() {
    struct command_info cmd;
    memset(&cmd, 0, sizeof(cmd));

    TEST_ASSERT_EQ(0, parse_command("echo hi > /tmp/out.txt", &cmd), "redirection should be parsed via shell operator path");
    free_command_info(&cmd);
    return 1;
}

static int test_contains_shell_operators() {
    ASSERT_FALSE(contains_shell_operators("echo hi"));
    ASSERT_TRUE(contains_shell_operators("echo hi | grep h"));
    ASSERT_TRUE(contains_shell_operators("echo hi > /tmp/out"));
    ASSERT_TRUE(contains_shell_operators("echo hi && echo bye"));
    ASSERT_TRUE(contains_shell_operators("echo $(date)"));
    return 1;
}

TEST_SUITE_BEGIN("Command Parsing Unit Tests")
    RUN_TEST(test_expand_equals_expression);
    RUN_TEST(test_parse_command_basic);
    RUN_TEST(test_parse_command_with_redirection_parsed_via_shell_ops);
    RUN_TEST(test_contains_shell_operators);
TEST_SUITE_END()

