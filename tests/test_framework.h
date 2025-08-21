#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>

/* Test framework macros */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, message); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(expected, actual, message) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "FAIL: %s:%d: %s (expected: %d, actual: %d)\n", \
                    __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_NE(expected, actual, message) \
    do { \
        if ((expected) == (actual)) { \
            fprintf(stderr, "FAIL: %s:%d: %s (expected not equal to: %d, actual: %d)\n", \
                    __FILE__, __LINE__, message, (int)(expected), (int)(actual)); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, message) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            fprintf(stderr, "FAIL: %s:%d: %s (expected: '%s', actual: '%s')\n", \
                    __FILE__, __LINE__, message, (expected), (actual)); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr, message) \
    do { \
        if ((ptr) != NULL) { \
            fprintf(stderr, "FAIL: %s:%d: %s (expected NULL, got %p)\n", \
                    __FILE__, __LINE__, message, (ptr)); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    do { \
        if ((ptr) == NULL) { \
            fprintf(stderr, "FAIL: %s:%d: %s (expected non-NULL, got NULL)\n", \
                    __FILE__, __LINE__, message); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

/* Simplified assertion macros for easier testing */
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            fprintf(stderr, "FAIL: %s:%d: Assertion failed: expected false but got true: %s\n", __FILE__, __LINE__, #condition); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            fprintf(stderr, "FAIL: %s:%d: Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            test_failures++; \
            return 0; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        printf("Running %s... ", #test_func); \
        fflush(stdout); \
        if (test_func()) { \
            printf("PASS\n"); \
            test_passes++; \
        } else { \
            printf("FAIL\n"); \
        } \
        test_count++; \
    } while(0)

#define TEST_SUITE_BEGIN(suite_name) \
    int main(void) { \
        int test_count = 0; \
        int test_passes = 0; \
        int test_failures = 0; \
        printf("=== %s ===\n", suite_name);

#define TEST_SUITE_END() \
        printf("\n=== Test Results ===\n"); \
        printf("Total tests: %d\n", test_count); \
        printf("Passed: %d\n", test_passes); \
        printf("Failed: %d\n", test_failures); \
        if (test_failures == 0) { \
            printf("All tests passed!\n"); \
            return 0; \
        } else { \
            printf("Some tests failed!\n"); \
            return 1; \
        } \
    }

/* Global test counters (declared in each test file) */
extern int test_count;
extern int test_passes;
extern int test_failures;

/* Test utility functions */

/**
 * Create a temporary file with given content
 */
static inline char *create_temp_file(const char *content) {
    static char template[] = "/tmp/sudosh_test_XXXXXX";
    char *filename = malloc(strlen(template) + 1);
    strcpy(filename, template);
    
    int fd = mkstemp(filename);
    if (fd == -1) {
        free(filename);
        return NULL;
    }
    
    if (content) {
        write(fd, content, strlen(content));
    }
    close(fd);
    
    return filename;
}

/**
 * Remove temporary file
 */
static inline void remove_temp_file(char *filename) {
    if (filename) {
        unlink(filename);
        free(filename);
    }
}

/**
 * Capture stdout/stderr from a function call
 */
typedef struct {
    char *stdout_content;
    char *stderr_content;
    int exit_code;
} capture_result_t;

/**
 * Execute a command and capture its output
 */
static inline capture_result_t *capture_command_output(const char *command) {
    capture_result_t *result = malloc(sizeof(capture_result_t));
    if (!result) return NULL;
    
    result->stdout_content = NULL;
    result->stderr_content = NULL;
    result->exit_code = -1;
    
    /* Create temporary files for stdout and stderr */
    char stdout_template[] = "/tmp/sudosh_stdout_XXXXXX";
    char stderr_template[] = "/tmp/sudosh_stderr_XXXXXX";
    
    int stdout_fd = mkstemp(stdout_template);
    int stderr_fd = mkstemp(stderr_template);
    
    if (stdout_fd == -1 || stderr_fd == -1) {
        if (stdout_fd != -1) close(stdout_fd);
        if (stderr_fd != -1) close(stderr_fd);
        free(result);
        return NULL;
    }
    
    close(stdout_fd);
    close(stderr_fd);
    
    /* Build command with redirections */
    char *full_command = malloc(strlen(command) + strlen(stdout_template) + strlen(stderr_template) + 20);
    sprintf(full_command, "%s >%s 2>%s", command, stdout_template, stderr_template);
    
    /* Execute command */
    result->exit_code = system(full_command);
    
    /* Read stdout */
    FILE *stdout_file = fopen(stdout_template, "r");
    if (stdout_file) {
        fseek(stdout_file, 0, SEEK_END);
        long stdout_size = ftell(stdout_file);
        fseek(stdout_file, 0, SEEK_SET);
        
        result->stdout_content = malloc(stdout_size + 1);
        fread(result->stdout_content, 1, stdout_size, stdout_file);
        result->stdout_content[stdout_size] = '\0';
        fclose(stdout_file);
    }
    
    /* Read stderr */
    FILE *stderr_file = fopen(stderr_template, "r");
    if (stderr_file) {
        fseek(stderr_file, 0, SEEK_END);
        long stderr_size = ftell(stderr_file);
        fseek(stderr_file, 0, SEEK_SET);
        
        result->stderr_content = malloc(stderr_size + 1);
        fread(result->stderr_content, 1, stderr_size, stderr_file);
        result->stderr_content[stderr_size] = '\0';
        fclose(stderr_file);
    }
    
    /* Clean up */
    unlink(stdout_template);
    unlink(stderr_template);
    free(full_command);
    
    return result;
}

/**
 * Free capture result
 */
static inline void free_capture_result(capture_result_t *result) {
    if (result) {
        free(result->stdout_content);
        free(result->stderr_content);
        free(result);
    }
}

#endif /* TEST_FRAMEWORK_H */
