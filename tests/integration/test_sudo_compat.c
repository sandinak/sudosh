#include "test_framework.h"
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int ensure_sudo_symlink(void) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) return 0;

    char target[PATH_MAX + 32];
    snprintf(target, sizeof(target), "%s/bin/sudosh", cwd);

    /* Remove existing symlink/file if any */
    unlink("/tmp/sudo");

    if (symlink(target, "/tmp/sudo") == 0) {
        return 1;
    }

    /* Fallback to shell ln -sf */
    char cmd[PATH_MAX * 2];
    snprintf(cmd, sizeof(cmd), "ln -sf '%s' /tmp/sudo", target);
    int rc = system(cmd);
    return (rc == 0);
}

/* Test that help shows sudo-compat banner when invoked as 'sudo' */
int test_sudo_compat_help_banner() {
    TEST_ASSERT(ensure_sudo_symlink(), "created /tmp/sudo symlink");
    capture_result_t *res = capture_command_output("PATH=/tmp:$PATH sudo --help");
    TEST_ASSERT_NOT_NULL(res, "help should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(res->exit_code), "help exits 0");
    TEST_ASSERT(res->stdout_content && strstr(res->stdout_content, "sudo-compat mode") != NULL,
                "help shows sudo-compat banner");
    free_capture_result(res);
    return 1;
}

/* Test -V prints version */
int test_sudo_compat_version_flag() {
    TEST_ASSERT(ensure_sudo_symlink(), "created /tmp/sudo symlink");
    capture_result_t *res = capture_command_output("PATH=/tmp:$PATH sudo -V");
    TEST_ASSERT_NOT_NULL(res, "-V should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(res->exit_code), "-V exits 0");
    TEST_ASSERT(res->stdout_content && strstr(res->stdout_content, "sudosh") != NULL,
                "-V prints version");
    free_capture_result(res);
    return 1;
}

/* Test -n -v refuses prompts (non-interactive) */
int test_sudo_compat_non_interactive_v() {
    TEST_ASSERT(ensure_sudo_symlink(), "created /tmp/sudo symlink");
    capture_result_t *res = capture_command_output("PATH=/tmp:$PATH sudo -n -v");
    TEST_ASSERT_NOT_NULL(res, "-n -v should execute");
    /* We expect non-zero (2) and a clear stderr message */
    TEST_ASSERT_NE(0, WEXITSTATUS(res->exit_code), "-n -v exits non-zero when auth needed");
    TEST_ASSERT(res->stdout_content || res->stderr_content, "has some output");
    if (res->stderr_content) {
        TEST_ASSERT(strstr(res->stderr_content, "non-interactive mode prevents authentication") != NULL,
                    "-n -v shows non-interactive error");
    }
    free_capture_result(res);
    return 1;
}

/* Test unsupported flags are rejected with a clear message (-p is supported) */
int test_sudo_compat_unsupported_flags() {
    TEST_ASSERT(ensure_sudo_symlink(), "created /tmp/sudo symlink");
    const char *flags[] = {"-E", "-H", "-i", "-s", "-A", "-S", "-b", NULL};
    for (int i = 0; flags[i]; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "PATH=/tmp:$PATH sudo %s", flags[i]);
        capture_result_t *res = capture_command_output(cmd);
        TEST_ASSERT_NOT_NULL(res, "flag should execute");
        TEST_ASSERT_NE(0, WEXITSTATUS(res->exit_code), "unsupported flag exits non-zero");
        TEST_ASSERT(res->stderr_content && strstr(res->stderr_content, "unsupported in sudo-compat mode") != NULL,
                    "unsupported flag error message");
        free_capture_result(res);
    }
    return 1;
}

/* Test -p sets the custom prompt without triggering auth (no-op in TEST_MODE) */
int test_sudo_compat_prompt_flag() {
    TEST_ASSERT(ensure_sudo_symlink(), "created /tmp/sudo symlink");
    capture_result_t *res = capture_command_output("PATH=/tmp:$PATH SUDOSH_TEST_MODE=1 sudo -p 'Password for %u@%h: ' --version");
    TEST_ASSERT_NOT_NULL(res, "-p with --version should execute");
    TEST_ASSERT_EQ(0, WEXITSTATUS(res->exit_code), "-p with --version exits 0");
    /* We can't capture internal state from here, but the command should not error */
    TEST_ASSERT(res->stdout_content && strstr(res->stdout_content, "sudosh") != NULL,
                "-p path still prints version");
    free_capture_result(res);
    return 1;
}

TEST_SUITE_BEGIN("Integration Tests - Sudo Compat Mode")
    RUN_TEST(test_sudo_compat_help_banner);
    RUN_TEST(test_sudo_compat_version_flag);
    RUN_TEST(test_sudo_compat_non_interactive_v);
    RUN_TEST(test_sudo_compat_unsupported_flags);
TEST_SUITE_END()

