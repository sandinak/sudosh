#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../test_framework.h"

/* Provide test counters expected by the framework */
int test_count = 0; int test_passes = 0; int test_failures = 0;

/* This test validates sudo-compat fallback policy behavior in test mode.
 * We require that when invoked as 'sudo' with a real command, sudosh does not
 * immediately exit due to fallback path, but actually runs the -c execution path.
 */

int test_sudo_compat_runs_c_command(void) {
    printf("Testing sudo-compat with -c executes via sudosh...\n");


    int pipefd[2];
    ASSERT_TRUE(pipe(pipefd) == 0);

    pid_t pid = fork();
    ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        setenv("SUDOSH_TEST_MODE", "1", 1);
        /* argv[0] deliberately set to "sudo" to enable sudo-compat path */
        execl("./bin/sudosh", "sudo", "-c", "echo sudosh-ok", (char*)NULL);
        _exit(127);
    }

    close(pipefd[1]);
    char buf[128] = {0};
    ssize_t n = read(pipefd[0], buf, sizeof(buf)-1);
    (void)n;
    close(pipefd[0]);
    int status; waitpid(pid, &status, 0);

    ASSERT_TRUE(strstr(buf, "sudosh-ok") != NULL);
    ASSERT_TRUE(WIFEXITED(status));
    return 1;
}

TEST_SUITE_BEGIN("Unit: sudo make install regression")
    RUN_TEST(test_sudo_compat_runs_c_command);
TEST_SUITE_END()

