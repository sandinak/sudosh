#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static const char *sudoers_fixture = \
    "testuser ALL=(ALL) NOPASSWD: /bin/ls, /usr/bin/head\n";

static int test_parse_sudoers_simple_rule() {
    char *tmp = create_temp_file(sudoers_fixture);
    TEST_ASSERT_NOT_NULL(tmp, "temp sudoers file created");

    struct sudoers_config *cfg = parse_sudoers_file(tmp);
    TEST_ASSERT_NOT_NULL(cfg, "parsed sudoers config");

    int allowed = check_sudoers_command_permission("testuser", "localhost", "/bin/ls", cfg);
    TEST_ASSERT_EQ(1, allowed, "user should be allowed /bin/ls");

    int allowed2 = check_sudoers_command_permission("testuser", "localhost", "/usr/bin/head", cfg);
    TEST_ASSERT_EQ(1, allowed2, "user should be allowed /usr/bin/head");

    int allowed3 = check_sudoers_command_permission("testuser", "localhost", "/bin/cat", cfg);
    TEST_ASSERT_EQ(0, allowed3, "user should not be allowed /bin/cat");

    free_sudoers_config(cfg);
    remove_temp_file(tmp);
    return 1;
}

TEST_SUITE_BEGIN("Sudoers Parsing Unit Tests")
    RUN_TEST(test_parse_sudoers_simple_rule);
TEST_SUITE_END()

