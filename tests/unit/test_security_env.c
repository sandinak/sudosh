#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_sanitize_environment_basic() {
    /* Set hazardous PATH and env vars */
    setenv("PATH", ":/usr/bin::", 1);
    setenv("LD_PRELOAD", "/tmp/malicious.so", 1);
    setenv("BASH_ENV", "/tmp/evil", 1);

    set_current_username("testuser");
    sanitize_environment();

    const char *path = getenv("PATH");
    TEST_ASSERT_NOT_NULL(path, "PATH should be set");
    ASSERT_FALSE(strstr(path, ".:") || strstr(path, ":.") || strstr(path, "::") || path[0] == ':' || path[strlen(path)-1] == ':');

    /* Dangerous variables should be gone */
    TEST_ASSERT_NULL(getenv("LD_PRELOAD"), "LD_PRELOAD should be unset");
    TEST_ASSERT_NULL(getenv("BASH_ENV"), "BASH_ENV should be unset");

    /* HOME/USER/LOGNAME set to root by policy */
    TEST_ASSERT_STR_EQ("/root", getenv("HOME"), "HOME should be /root");
    TEST_ASSERT_STR_EQ("root", getenv("USER"), "USER should be root");
    TEST_ASSERT_STR_EQ("root", getenv("LOGNAME"), "LOGNAME should be root");

    return 1;
}

TEST_SUITE_BEGIN("Security Environment Tests")
    RUN_TEST(test_sanitize_environment_basic);
TEST_SUITE_END()

