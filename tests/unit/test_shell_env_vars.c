#include "../tests/test_framework.h"
#include "../src/sudosh.h"
#include <stdlib.h>
#include <string.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_validate_env_var_name() {
    TEST_ASSERT(validate_env_var_name("FOO") == 1, "valid name FOO");
    TEST_ASSERT(validate_env_var_name("_BAR") == 1, "valid name _BAR");
    TEST_ASSERT(validate_env_var_name("1BAD") == 0, "invalid starts with digit");
    TEST_ASSERT(validate_env_var_name("BAD-NAME") == 0, "invalid hyphen");
    return 1;
}

static int test_export_safe_var() {
    unsetenv("LESS");
    int ok = handle_export_command("export LESS=more");
    TEST_ASSERT(ok == 1, "export safe var LESS");
    const char *v = getenv("LESS");
    TEST_ASSERT(v && strcmp(v, "more") == 0, "LESS set to more");
    return 1;
}

static int test_export_dangerous_var_blocked() {
    int ok = handle_export_command("export PATH=/tmp");
    TEST_ASSERT(ok == 0, "export PATH should be blocked");
    return 1;
}

static int test_unset_safe_var() {
    setenv("PAGER", "less", 1);
    int ok = handle_unset_command("unset PAGER");
    TEST_ASSERT(ok == 1, "unset safe var PAGER");
    return 1;
}

static int test_unset_dangerous_var_blocked() {
    setenv("PATH", "/usr/bin", 1);
    int ok = handle_unset_command("unset PATH");
    TEST_ASSERT(ok == 0, "unset PATH should be blocked");
    return 1;
}

TEST_SUITE_BEGIN("Shell Env Management Unit Tests")
    RUN_TEST(test_validate_env_var_name);
    RUN_TEST(test_export_safe_var);
    RUN_TEST(test_export_dangerous_var_blocked);
    RUN_TEST(test_unset_safe_var);
    RUN_TEST(test_unset_dangerous_var_blocked);
TEST_SUITE_END()

