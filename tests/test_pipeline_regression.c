#include "test_framework.h"
#include "sudosh.h"

/* Global test counters used by the macros */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_pipeline_detection(void) {
    TEST_ASSERT(is_pipeline_command("ps | grep root") == 1, "pipeline with one pipe should be detected");
    TEST_ASSERT(is_pipeline_command("ls -la") == 0, "non-pipeline command should not be detected as pipeline");
    TEST_ASSERT(is_pipeline_command("echo foo | wc -l | cat") == 1, "multi-pipe command should be detected as pipeline");
    return 1;
}

static int test_whitelist_integrity_basic(void) {
    /* Allowed pipeline-safe commands */
    TEST_ASSERT(is_whitelisted_pipe_command("ps") == 1, "ps should be whitelisted for pipeline use");
    TEST_ASSERT(is_whitelisted_pipe_command("grep") == 1, "grep should be whitelisted for pipeline use");
    TEST_ASSERT(is_whitelisted_pipe_command("cat") == 1, "cat should be whitelisted for pipeline use");

    /* Dangerous commands should never be whitelisted */
    TEST_ASSERT(is_whitelisted_pipe_command("rm") == 0, "rm must not be whitelisted");
    TEST_ASSERT(is_whitelisted_pipe_command("chmod") == 0, "chmod must not be whitelisted");
    TEST_ASSERT(is_whitelisted_pipe_command("sudo") == 0, "sudo must not be whitelisted");
    return 1;
}

static int test_secure_pager_environment_setup(void) {
    setup_secure_pager_environment();
    const char *lesssecure = getenv("LESSSECURE");
    const char *lessopen = getenv("LESSOPEN");
    const char *visual = getenv("VISUAL");

    TEST_ASSERT(lesssecure != NULL && strcmp(lesssecure, "1") == 0, "LESSSECURE should be set to 1");
    TEST_ASSERT(lessopen != NULL && strcmp(lessopen, "") == 0, "LESSOPEN should be disabled");
    TEST_ASSERT(visual != NULL && strcmp(visual, "/bin/false") == 0, "VISUAL should be disabled");
    return 1;
}

int main(void) {
    /* Initialize any test-mode defaults expected by code under test */
    test_mode = 1; /* ensure test paths are taken where applicable */

    RUN_TEST(test_pipeline_detection);
    RUN_TEST(test_whitelist_integrity_basic);
    RUN_TEST(test_secure_pager_environment_setup);

    printf("\n=== Pipeline Regression Tests (basic) ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);

    return (test_failures == 0) ? 0 : 1;
}

