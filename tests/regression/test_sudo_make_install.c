#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../test_framework.h"
/* Provide test counters expected by the framework */
int test_count = 0; int test_passes = 0; int test_failures = 0;
#include "../../src/sudosh.h"

/* Regression: invoking sudosh as sudo (argv[0]=="sudo") with a real command
 * should not fallback to /usr/bin/sudo even if not setuid, it should attempt
 * to execute via sudosh policy path so that 'sudosh make install' works. */

int test_sudo_compat_executes_commands(void) {
    printf("Testing sudo-compat executes commands without fallback...\n");

    /* Simulate sudo-compat: exec ./bin/sudosh with argv[0]="sudo" and args 'echo ok' */
    char *argv0 = (char*)"sudo";
    char *args[] = { argv0, (char*)"-c", (char*)"echo ok", NULL };

    int pipefd[2];
    ASSERT_TRUE(pipe(pipefd) == 0);

    pid_t pid = fork();
    ASSERT_TRUE(pid >= 0);

    if (pid == 0) {
        /* Child: redirect stdout to pipe and exec */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]); close(pipefd[1]);
        /* Ensure test mode for bypassing privilege checks */
        setenv("SUDOSH_TEST_MODE", "1", 1);
        execv("./bin/sudosh", args);
        _exit(127);
    }

    /* Parent */
    close(pipefd[1]);
    char buf[64] = {0};
    ssize_t n = read(pipefd[0], buf, sizeof(buf)-1);
    (void)n;
    close(pipefd[0]);
    int status; waitpid(pid, &status, 0);

    /* We expect child to print ok (through sudosh -c path) */
    ASSERT_TRUE(strstr(buf, "ok") != NULL);
    ASSERT_TRUE(WIFEXITED(status));
    return 1;
}

TEST_SUITE_BEGIN("Regression: sudo make install via sudosh")
    RUN_TEST(test_sudo_compat_executes_commands);
TEST_SUITE_END()

