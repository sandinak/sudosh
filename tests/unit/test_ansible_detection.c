#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <string.h>
#include <stdio.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

extern int ansible_detection_force;

static void clear_global_ansible(void) {
    if (global_ansible_info) {
        free_ansible_detection_info(global_ansible_info);
        global_ansible_info = NULL;
    }
}

static int test_ansible_env_vars_detected() {
    clear_global_ansible();
    /* Set become method to ensure high-confidence detection */
    setenv("ANSIBLE_BECOME_METHOD", "sudosh", 1);
    struct ansible_detection_info *info = detect_ansible_session();
    ASSERT_TRUE(info != NULL);
    ASSERT_TRUE(info->is_ansible_session == 1);
    clear_global_ansible();
    unsetenv("ANSIBLE_BECOME_METHOD");
    return 1;
}

static int test_ansible_validate_command_passthrough() {
    clear_global_ansible();
    setenv("ANSIBLE_RUNNER", "1", 1);
    ansible_detection_force = 1;
    /* Ensure validate_ansible_command returns success (0) for benign command */
    /* For ansible sessions, validate_ansible_command() allows safe whitelisted commands */
    int rc = validate_ansible_command("echo", "testuser");
    TEST_ASSERT_EQ(1, rc, "validate_ansible_command should allow benign commands under ansible context");
    clear_global_ansible();
    ansible_detection_force = 0;
    unsetenv("ANSIBLE_RUNNER");
    return 1;
}

TEST_SUITE_BEGIN("Ansible Detection Unit Tests")
    RUN_TEST(test_ansible_env_vars_detected);
    RUN_TEST(test_ansible_validate_command_passthrough);
TEST_SUITE_END()

