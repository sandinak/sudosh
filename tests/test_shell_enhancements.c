#include "test_framework.h"
#include "sudosh.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test tab completion functionality */
int test_tab_completion() {
    printf("Running test_tab_completion... ");
    
    /* Test find_completion_start function */
    char *prefix = find_completion_start("ls /usr/b", 9);
    TEST_ASSERT_NOT_NULL(prefix, "find_completion_start should return a prefix");
    TEST_ASSERT_STR_EQ("/usr/b", prefix, "prefix should be '/usr/b'");
    free(prefix);
    
    /* Test with no prefix */
    prefix = find_completion_start("ls ", 3);
    TEST_ASSERT_NOT_NULL(prefix, "find_completion_start should return empty prefix");
    TEST_ASSERT_STR_EQ("", prefix, "prefix should be empty");
    free(prefix);
    
    /* Test complete_path function with /usr directory */
    char **matches = complete_path("/usr/b", 0, 6, 0);
    if (matches) {
        /* Should find /usr/bin at minimum on most systems */
        int found_bin = 0;
        for (int i = 0; matches[i]; i++) {
            if (strstr(matches[i], "bin")) {
                found_bin = 1;
                break;
            }
        }
        TEST_ASSERT_EQ(1, found_bin, "should find bin directory in /usr");

        /* Free matches */
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
    }

    /* Test complete_path with current directory */
    matches = complete_path(".", 0, 1, 0);
    if (matches) {
        /* Should find at least one match */
        TEST_ASSERT_NOT_NULL(matches[0], "should find at least one match in current directory");
        
        /* Free matches */
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
    }
    
    printf("PASS\n");
    return 1;
}

/* Test Ctrl-D handling */
int test_ctrl_d_handling() {
    printf("Running test_ctrl_d_handling... ");
    
    /* This test verifies that the read_command function properly handles EOF */
    /* We can't easily test interactive input, but we can test the logic */
    
    /* Create a pipe to simulate EOF input */
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        printf("FAIL (pipe creation failed)\n");
        return 0;
    }
    
    /* Close write end to simulate EOF */
    close(pipefd[1]);
    
    /* Save stdin and redirect to our pipe */
    int saved_stdin = dup(STDIN_FILENO);
    dup2(pipefd[0], STDIN_FILENO);
    
    /* This should return NULL due to EOF */
    char *result = read_command();
    TEST_ASSERT_NULL(result, "read_command should return NULL on EOF");
    
    /* Restore stdin */
    dup2(saved_stdin, STDIN_FILENO);
    close(saved_stdin);
    close(pipefd[0]);
    
    printf("PASS\n");
    return 1;
}

/* Test signal handling improvements */
int test_signal_handling() {
    printf("Running test_signal_handling... ");

    /* Test signal setup */
    setup_signal_handlers();

    /* Test that SIGPIPE is ignored after setup */
    struct sigaction sa;
    if (sigaction(SIGPIPE, NULL, &sa) == 0) {
        TEST_ASSERT_EQ((long)SIG_IGN, (long)sa.sa_handler, "SIGPIPE should be ignored after setup");
    }

    /* Verify signal handlers are set */
    if (sigaction(SIGINT, NULL, &sa) == 0) {
        TEST_ASSERT_NE((long)SIG_DFL, (long)sa.sa_handler, "SIGINT should have custom handler");
    }

    if (sigaction(SIGTERM, NULL, &sa) == 0) {
        TEST_ASSERT_NE((long)SIG_DFL, (long)sa.sa_handler, "SIGTERM should have custom handler");
    }

    printf("PASS\n");
    return 1;
}

/* Test command execution with signal handling */
int test_command_execution_signals() {
    printf("Running test_command_execution_signals... ");

    /* Test parsing commands (this doesn't require root privileges) */
    struct command_info cmd;
    int result = parse_command("echo test", &cmd);
    TEST_ASSERT_EQ(0, result, "parse_command should succeed");

    /* Check that command was parsed correctly */
    TEST_ASSERT_NOT_NULL(cmd.argv, "argv should not be NULL");
    TEST_ASSERT_NOT_NULL(cmd.argv[0], "argv[0] should not be NULL");
    TEST_ASSERT_STR_EQ("echo", cmd.argv[0], "first argument should be 'echo'");
    TEST_ASSERT_STR_EQ("test", cmd.argv[1], "second argument should be 'test'");

    free_command_info(&cmd);

    /* Test with a more complex command */
    result = parse_command("ls -la /tmp", &cmd);
    TEST_ASSERT_EQ(0, result, "parse_command should succeed for complex commands");
    TEST_ASSERT_EQ(3, cmd.argc, "argc should be 3");

    free_command_info(&cmd);

    /* Test with empty command */
    result = parse_command("", &cmd);
    TEST_ASSERT_EQ(0, result, "parse_command should handle empty commands");
    TEST_ASSERT_EQ(0, cmd.argc, "argc should be 0 for empty command");

    free_command_info(&cmd);

    printf("PASS\n");
    return 1;
}

/* Test directory structure organization */
int test_directory_structure() {
    printf("Running test_directory_structure... ");
    
    /* Test that source files are in src/ directory */
    TEST_ASSERT_EQ(0, access("src/main.c", F_OK), "src/main.c should exist");
    TEST_ASSERT_EQ(0, access("src/sudosh.h", F_OK), "src/sudosh.h should exist");
    TEST_ASSERT_EQ(0, access("src/utils.c", F_OK), "src/utils.c should exist");
    TEST_ASSERT_EQ(0, access("src/command.c", F_OK), "src/command.c should exist");
    
    /* Test that docs are in docs/ directory */
    TEST_ASSERT_EQ(0, access("docs/README.md", F_OK), "docs/README.md should exist");
    TEST_ASSERT_EQ(0, access("docs/sudosh.1.in", F_OK), "docs/sudosh.1.in should exist");
    
    /* Test that tests are in tests/ directory */
    TEST_ASSERT_EQ(0, access("tests/test_framework.h", F_OK), "tests/test_framework.h should exist");
    
    /* Test that binary is built in bin/ directory */
    TEST_ASSERT_EQ(0, access("bin/sudosh", F_OK), "bin/sudosh should exist");
    
    printf("PASS\n");
    return 1;
}

/* Test insert_completion function */
int test_insert_completion() {
    printf("Running test_insert_completion... ");
    
    char buffer[1024] = "ls /usr/b";
    int pos = 9;
    int len = 9;
    
    /* Test inserting completion */
    insert_completion(buffer, &pos, &len, "/usr/bin/", "/usr/b");
    
    TEST_ASSERT_STR_EQ("ls /usr/bin/", buffer, "completion should be inserted correctly");
    TEST_ASSERT_EQ(12, pos, "position should be updated");
    TEST_ASSERT_EQ(12, len, "length should be updated");
    
    printf("PASS\n");
    return 1;
}

int main() {
    printf("=== Shell Enhancements Tests ===\n");
    
    /* Run all tests */
    test_passes += test_tab_completion();
    test_passes += test_ctrl_d_handling();
    test_passes += test_signal_handling();
    test_passes += test_command_execution_signals();
    test_passes += test_directory_structure();
    test_passes += test_insert_completion();
    
    test_count = 6;
    test_failures = test_count - test_passes;
    
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passes);
    printf("Failed: %d\n", test_failures);
    
    if (test_failures == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("Some tests failed!\n");
        return 1;
    }
}
