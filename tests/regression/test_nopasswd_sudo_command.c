#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <pwd.h>
#include "../test_framework.h"

/* Regression: invoking via sudo-compat with a real command should skip auth when NOPASSWD: ALL is present */
int test_nopasswd_sudo_runs_command_without_prompt() {
    /* Create a 'sudo' symlink to sudosh in /tmp so argv[0]==sudo */
    char cwd[PATH_MAX];
    TEST_ASSERT(getcwd(cwd, sizeof(cwd)) != NULL, "getcwd ok");
    char target[PATH_MAX + 32];
    snprintf(target, sizeof(target), "%s/bin/sudosh", cwd);
    unlink("/tmp/sudo");
    TEST_ASSERT(symlink(target, "/tmp/sudo") == 0 || access("/tmp/sudo", F_OK) == 0, "symlink sudo -> sudosh");

    /* Prepare a temporary sudoers file granting NOPASSWD: ALL to current user */
    const char *user = getenv("SUDO_USER");
    if (!user || !*user) {
        struct passwd *pw = getpwuid(getuid());
        TEST_ASSERT(pw && pw->pw_name, "resolve current username");
        user = pw->pw_name;
    }
    char sudoers_content[256];
    snprintf(sudoers_content, sizeof(sudoers_content), "%s ALL=(ALL) NOPASSWD: ALL\n", user);
    char *sudoers_path = create_temp_file(sudoers_content);
    TEST_ASSERT_NOT_NULL(sudoers_path, "temp sudoers created");

    /* Provide an empty include dir to avoid scanning system dirs */
    char tmpdir_tpl[] = "/tmp/sudosh_inc_XXXXXX";
    char *incdir = mkdtemp(tmpdir_tpl);
    TEST_ASSERT(incdir != NULL, "created temp include dir");

    /* Clear auth cache env and point parser to our test sudoers */
    unsetenv("SUDOSH_AUTH_CACHE");
    setenv("SUDOSH_SUDOERS_PATH", sudoers_path, 1);
    setenv("SUDOSH_SUDOERS_DIR", incdir, 1);

    /* Run sudo with a benign command; expect success and no auth failure */
    capture_result_t *res = capture_command_output("SUDOSH_TEST_MODE=1 PATH=/tmp:$PATH sudo /bin/echo ok");
    TEST_ASSERT_NOT_NULL(res, "command executed");
    TEST_ASSERT_EQ(0, WEXITSTATUS(res->exit_code), "sudo compat executes without auth under NOPASSWD");
    if (res->stderr_content) {
        TEST_ASSERT(strstr(res->stderr_content, "authentication failed") == NULL, "no auth failure when NOPASSWD");
    }
    if (res->stdout_content) {
        TEST_ASSERT(strstr(res->stdout_content, "ok") != NULL, "command output present");
    }

    /* Cleanup */
    free_capture_result(res);
    remove_temp_file(sudoers_path);
    rmdir(incdir);
    unlink("/tmp/sudo");
    return 1;
}

int test_count = 0; int test_passes = 0; int test_failures = 0;
TEST_SUITE_BEGIN("Regression - NOPASSWD sudo command execution")
    RUN_TEST(test_nopasswd_sudo_runs_command_without_prompt);
TEST_SUITE_END()

