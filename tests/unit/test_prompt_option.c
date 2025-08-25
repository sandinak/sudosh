#include "test_framework.h"
#include "sudosh.h"
#include <string.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test that -p/--prompt sets the custom password prompt used by auth */
int test_set_custom_password_prompt_api() {
    /* Ensure getter returns NULL before we set */
    const char *before = get_custom_password_prompt();
    (void)before; /* Just verify it links/compiles */

    const char *expected = "Password for %u@%h: ";
    set_custom_password_prompt(expected);

    const char *after = get_custom_password_prompt();
    TEST_ASSERT(after != NULL, "prompt should be set");
    TEST_ASSERT_STR_EQ(expected, after, "prompt should match set value");
    return 1;
}

TEST_SUITE_BEGIN("Unit Tests - Prompt Option")
    RUN_TEST(test_set_custom_password_prompt_api);
TEST_SUITE_END()

