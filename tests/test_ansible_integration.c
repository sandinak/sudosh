/**
 * test_ansible_integration.c - Ansible Become Plugin Integration Tests
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Comprehensive test suite for the Ansible become plugin functionality
 * including command validation, JSON output, and security constraints.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>

/* Include sudosh headers */
#include "../src/sudosh.h"

/* Global variables are now defined in globals.c */
char *session_logfile = NULL;

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test helper macros */
#define TEST_START(name) \
    do { \
        printf("Running test: %s... ", name); \
        fflush(stdout); \
        tests_run++; \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS\n"); \
        tests_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        tests_failed++; \
    } while(0)

#define ASSERT_TRUE(condition, msg) \
    do { \
        if (!(condition)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(condition, msg) \
    do { \
        if (condition) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

/**
 * Execute sudosh command and capture output
 */
static int execute_sudosh_command(const char *args, char **output, int *exit_code) {
    char command[1024];
    FILE *fp;
    char buffer[4096];
    size_t output_len = 0;
    
    snprintf(command, sizeof(command), "./bin/sudosh %s 2>&1", args);
    
    fp = popen(command, "r");
    if (!fp) {
        return -1;
    }
    
    *output = malloc(1);
    (*output)[0] = '\0';
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t buffer_len = strlen(buffer);
        size_t new_len = output_len + buffer_len + 1;
        char *new_output = realloc(*output, new_len);
        if (!new_output) {
            free(*output);
            pclose(fp);
            return -1;
        }
        *output = new_output;
        strcpy(*output + output_len, buffer);
        output_len += buffer_len;
    }
    
    *exit_code = pclose(fp);
    return 0;
}

/**
 * Test basic Ansible mode functionality
 */
static void test_ansible_mode_basic() {
    TEST_START("ansible_mode_basic");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--ansible-mode --command \"pwd\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\"") != NULL, "JSON output missing success field");
    ASSERT_TRUE(strstr(output, "\"exit_code\"") != NULL, "JSON output missing exit_code field");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode with allowed command
 */
static void test_ansible_allowed_command() {
    TEST_START("ansible_allowed_command");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--ansible-mode --command \"ls /tmp\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\": true") != NULL, "Command should have succeeded");
    ASSERT_TRUE(strstr(output, "\"stdout\"") != NULL, "Should have stdout output");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode with blocked command
 */
static void test_ansible_blocked_command() {
    TEST_START("ansible_blocked_command");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--ansible-mode --command \"rm -rf /\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\": false") != NULL, "Dangerous command should be blocked");
    ASSERT_TRUE(strstr(output, "\"error\"") != NULL, "Should have error message");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode JSON output format
 */
static void test_ansible_json_format() {
    TEST_START("ansible_json_format");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--ansible-mode --command \"whoami\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    
    /* Check for valid JSON structure */
    ASSERT_TRUE(output[0] == '{', "Output should start with {");
    ASSERT_TRUE(strstr(output, "}") != NULL, "Output should end with }");
    ASSERT_TRUE(strstr(output, "\"success\":") != NULL, "Missing success field");
    ASSERT_TRUE(strstr(output, "\"exit_code\":") != NULL, "Missing exit_code field");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode with secure editor
 */
static void test_ansible_secure_editor() {
    TEST_START("ansible_secure_editor");
    
    char *output = NULL;
    int exit_code = 0;
    
    /* Create a test file */
    system("echo 'test content' > /tmp/ansible_test.txt");
    
    int result = execute_sudosh_command("--ansible-mode --command \"vi --version\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\": true") != NULL, "vi should be allowed");
    
    /* Clean up */
    system("rm -f /tmp/ansible_test.txt");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode with dangerous editor
 */
static void test_ansible_dangerous_editor() {
    TEST_START("ansible_dangerous_editor");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--ansible-mode --command \"emacs /tmp/test.txt\"", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\": false") != NULL, "emacs should be blocked");
    ASSERT_TRUE(strstr(output, "\"error\"") != NULL, "Should have error message");
    
    free(output);
    TEST_PASS();
}

/**
 * Test Ansible mode parameter parsing
 */
static void test_ansible_parameter_parsing() {
    TEST_START("ansible_parameter_parsing");

    char *output = NULL;
    int exit_code = 0;

    int result = execute_sudosh_command("--ansible-mode --command \"id\"", &output, &exit_code);

    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "\"success\"") != NULL, "Should have success field");

    free(output);
    TEST_PASS();
}

/**
 * Test that regular mode still works
 */
static void test_regular_mode_still_works() {
    TEST_START("regular_mode_still_works");
    
    char *output = NULL;
    int exit_code = 0;
    
    int result = execute_sudosh_command("--help", &output, &exit_code);
    
    ASSERT_TRUE(result == 0, "Failed to execute command");
    ASSERT_TRUE(output != NULL, "No output received");
    ASSERT_TRUE(strstr(output, "sudosh - Interactive sudo shell") != NULL, "Should show help message");
    ASSERT_TRUE(strstr(output, "--ansible-mode") != NULL, "Should show ansible-mode option");
    
    free(output);
    TEST_PASS();
}

/**
 * Run all tests
 */
int main() {
    printf("=== Ansible Integration Test Suite ===\n\n");
    
    /* Run all tests */
    test_ansible_mode_basic();
    test_ansible_allowed_command();
    test_ansible_blocked_command();
    test_ansible_json_format();
    test_ansible_secure_editor();
    test_ansible_dangerous_editor();
    test_ansible_parameter_parsing();
    test_regular_mode_still_works();
    
    /* Print summary */
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n✅ All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ %d test(s) failed!\n", tests_failed);
        return 1;
    }
}
