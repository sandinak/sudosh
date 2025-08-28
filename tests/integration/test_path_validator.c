#include "test_framework.h"
#include "sudosh.h"
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_path_validator_quiet_secure() {
    capture_result_t *result = capture_command_output("./bin/path-validator -q -p \"/bin:/usr/bin\"");
    TEST_ASSERT_NOT_NULL(result, "path-validator should execute in quiet mode");
    TEST_ASSERT_EQ(0, WEXITSTATUS(result->exit_code), "secure PATH should exit 0");
    free_capture_result(result);
    return 1;
}

static int test_path_validator_quiet_insecure() {
    capture_result_t *result = capture_command_output("./bin/path-validator -q -p \".:/usr/bin\"");
    TEST_ASSERT_NOT_NULL(result, "path-validator should execute in quiet mode");
    TEST_ASSERT_EQ(1, WEXITSTATUS(result->exit_code), "insecure PATH should exit 1");
    free_capture_result(result);
    return 1;
}

static int test_path_validator_clean_output() {
    capture_result_t *result = capture_command_output("./bin/path-validator -c -p \".:/bin::/usr/bin\"");
    TEST_ASSERT_NOT_NULL(result, "path-validator -c should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(result->exit_code), "-c should exit 0");
    TEST_ASSERT(result->stdout_content != NULL, "should print cleaned PATH");
    /* Cleaned PATH should not contain .: or :: and should start with '/' */
    TEST_ASSERT(strstr(result->stdout_content, ".:") == NULL, "cleaned PATH should not contain '.:'");
    TEST_ASSERT(strstr(result->stdout_content, "::") == NULL, "cleaned PATH should not contain '::'");
    TEST_ASSERT(result->stdout_content[0] == '/', "cleaned PATH should start with '/'");
    free_capture_result(result);
    return 1;
}

static int test_path_validator_help() {
    capture_result_t *result = capture_command_output("./bin/path-validator --help");
    TEST_ASSERT_NOT_NULL(result, "--help should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(result->exit_code), "--help should exit 0");
    TEST_ASSERT(strstr(result->stdout_content, "Usage:") != NULL, "help should contain Usage:");
    free_capture_result(result);
    return 1;
}

TEST_SUITE_BEGIN("Path Validator Integration Tests")
    RUN_TEST(test_path_validator_quiet_secure);
    RUN_TEST(test_path_validator_quiet_insecure);
    RUN_TEST(test_path_validator_clean_output);
    RUN_TEST(test_path_validator_help);
TEST_SUITE_END()

