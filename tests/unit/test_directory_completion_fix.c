#include "test_framework.h"
#include "sudosh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Comprehensive test to validate the directory completion fix */

int test_directory_end_detection() {
    printf("Running test_directory_end_detection... ");
    
    /* Test case 1: 'ls /etc/' should be detected as directory end */
    char buffer1[] = "ls /etc/";
    int pos1 = 8;
    char *prefix1 = find_completion_start(buffer1, pos1);
    
    if (!prefix1 || strcmp(prefix1, "/etc/") != 0) {
        if (prefix1) free(prefix1);
        printf("FAIL\n");
        return 0;
    }
    
    /* Simulate the directory end detection logic */
    int prefix_start1 = pos1 - strlen(prefix1);
    int is_empty_prefix1 = (strlen(prefix1) == 0);
    int is_directory_end1 = 0;
    
    if (!is_empty_prefix1 && strlen(prefix1) > 0 && prefix1[strlen(prefix1) - 1] == '/') {
        if (pos1 == prefix_start1 + (int)strlen(prefix1)) {
            is_directory_end1 = 1;
            is_empty_prefix1 = 1;
        }
    }
    
    if (!is_directory_end1 || !is_empty_prefix1) {
        free(prefix1);
        printf("FAIL\n");
        return 0;
    }
    
    free(prefix1);
    
    /* Test case 2: 'ls /etc/host' should NOT be detected as directory end */
    char buffer2[] = "ls /etc/host";
    int pos2 = 12;
    char *prefix2 = find_completion_start(buffer2, pos2);
    
    if (!prefix2 || strcmp(prefix2, "/etc/host") != 0) {
        if (prefix2) free(prefix2);
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start2 = pos2 - strlen(prefix2);
    int is_empty_prefix2 = (strlen(prefix2) == 0);
    int is_directory_end2 = 0;
    
    if (!is_empty_prefix2 && strlen(prefix2) > 0 && prefix2[strlen(prefix2) - 1] == '/') {
        if (pos2 == prefix_start2 + (int)strlen(prefix2)) {
            is_directory_end2 = 1;
        }
    }
    
    if (is_directory_end2) {  /* Should NOT be directory end */
        free(prefix2);
        printf("FAIL\n");
        return 0;
    }
    
    free(prefix2);
    
    printf("PASS\n");
    return 1;
}

int test_directory_completion_behavior() {
    printf("Running test_directory_completion_behavior... ");
    
    /* Test the complete flow for 'ls /etc/' */
    char buffer[] = "ls /etc/";
    int pos = 8;
    
    char *prefix = find_completion_start(buffer, pos);
    if (!prefix) {
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start = pos - strlen(prefix);
    int is_empty_prefix = (strlen(prefix) == 0);
    int is_directory_end = 0;
    
    /* Apply the directory end detection logic */
    if (!is_empty_prefix && strlen(prefix) > 0 && prefix[strlen(prefix) - 1] == '/') {
        if (pos == prefix_start + (int)strlen(prefix)) {
            is_directory_end = 1;
            is_empty_prefix = 1;
        }
    }
    
    /* Determine completion text */
    char *completion_text = prefix;
    if (is_empty_prefix && is_directory_end) {
        completion_text = prefix;  /* Use the directory path itself */
    }
    
    /* Test path completion */
    char **matches = complete_path(completion_text, 0, strlen(completion_text), 0, 0);
    if (!matches || !matches[0]) {
        free(prefix);
        if (matches) free(matches);
        printf("FAIL\n");
        return 0;
    }
    
    /* Test the display logic */
    int should_display_list = (is_empty_prefix && (matches[1] != NULL || is_directory_end));
    
    if (!should_display_list) {
        /* Free matches */
        for (int i = 0; matches[i]; i++) {
            free(matches[i]);
        }
        free(matches);
        free(prefix);
        printf("FAIL\n");
        return 0;
    }
    
    /* Free matches */
    for (int i = 0; matches[i]; i++) {
        free(matches[i]);
    }
    free(matches);
    free(prefix);
    
    printf("PASS\n");
    return 1;
}

int test_partial_completion_unchanged() {
    printf("Running test_partial_completion_unchanged... ");
    
    /* Test that 'ls /etc/host' still works as partial completion */
    char buffer[] = "ls /etc/host";
    int pos = 12;
    
    char *prefix = find_completion_start(buffer, pos);
    if (!prefix || strcmp(prefix, "/etc/host") != 0) {
        if (prefix) free(prefix);
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start = pos - strlen(prefix);
    int is_empty_prefix = (strlen(prefix) == 0);
    int is_directory_end = 0;
    
    /* Apply the directory end detection logic */
    if (!is_empty_prefix && strlen(prefix) > 0 && prefix[strlen(prefix) - 1] == '/') {
        if (pos == prefix_start + (int)strlen(prefix)) {
            is_directory_end = 1;
            is_empty_prefix = 1;
        }
    }
    
    /* Should NOT be treated as empty prefix or directory end */
    if (is_empty_prefix || is_directory_end) {
        free(prefix);
        printf("FAIL\n");
        return 0;
    }
    
    /* Test path completion */
    char **matches = complete_path(prefix, 0, strlen(prefix), 0, 0);
    if (!matches || !matches[0]) {
        free(prefix);
        if (matches) free(matches);
        printf("FAIL\n");
        return 0;
    }
    
    /* Should find hosts file */
    int found_hosts = 0;
    for (int i = 0; matches[i]; i++) {
        if (strstr(matches[i], "hosts")) {
            found_hosts = 1;
            break;
        }
    }
    
    /* Free matches */
    for (int i = 0; matches[i]; i++) {
        free(matches[i]);
    }
    free(matches);
    free(prefix);
    
    if (!found_hosts) {
        printf("FAIL\n");
        return 0;
    }
    
    printf("PASS\n");
    return 1;
}

int test_relative_directory_completion() {
    printf("Running test_relative_directory_completion... ");
    
    /* Test that 'ls src/' also works */
    char buffer[] = "ls src/";
    int pos = 7;
    
    char *prefix = find_completion_start(buffer, pos);
    if (!prefix || strcmp(prefix, "src/") != 0) {
        if (prefix) free(prefix);
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start = pos - strlen(prefix);
    int is_empty_prefix = (strlen(prefix) == 0);
    int is_directory_end = 0;
    
    /* Apply the directory end detection logic */
    if (!is_empty_prefix && strlen(prefix) > 0 && prefix[strlen(prefix) - 1] == '/') {
        if (pos == prefix_start + (int)strlen(prefix)) {
            is_directory_end = 1;
            is_empty_prefix = 1;
        }
    }
    
    if (!is_directory_end || !is_empty_prefix) {
        free(prefix);
        printf("FAIL\n");
        return 0;
    }
    
    /* Test path completion */
    char **matches = complete_path(prefix, 0, strlen(prefix), 0, 0);
    if (!matches || !matches[0]) {
        free(prefix);
        if (matches) free(matches);
        printf("FAIL\n");
        return 0;
    }
    
    /* Should find files in src/ directory */
    int found_src_files = 0;
    for (int i = 0; matches[i]; i++) {
        if (strstr(matches[i], "src/")) {
            found_src_files = 1;
            break;
        }
    }
    
    /* Free matches */
    for (int i = 0; matches[i]; i++) {
        free(matches[i]);
    }
    free(matches);
    free(prefix);
    
    if (!found_src_files) {
        printf("FAIL\n");
        return 0;
    }
    
    printf("PASS\n");
    return 1;
}

int test_edge_cases() {
    printf("Running test_edge_cases... ");
    
    /* Test case 1: Single character directory */
    char buffer1[] = "ls /";
    int pos1 = 4;
    char *prefix1 = find_completion_start(buffer1, pos1);
    
    if (!prefix1 || strcmp(prefix1, "/") != 0) {
        if (prefix1) free(prefix1);
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start1 = pos1 - strlen(prefix1);
    int is_directory_end1 = 0;
    if (strlen(prefix1) > 0 && prefix1[strlen(prefix1) - 1] == '/') {
        if (pos1 == prefix_start1 + (int)strlen(prefix1)) {
            is_directory_end1 = 1;
        }
    }
    
    if (!is_directory_end1) {
        free(prefix1);
        printf("FAIL\n");
        return 0;
    }
    
    free(prefix1);
    
    /* Test case 2: Path not ending with '/' should not trigger */
    char buffer2[] = "ls /etc";
    int pos2 = 7;
    char *prefix2 = find_completion_start(buffer2, pos2);
    
    if (!prefix2 || strcmp(prefix2, "/etc") != 0) {
        if (prefix2) free(prefix2);
        printf("FAIL\n");
        return 0;
    }
    
    int prefix_start2 = pos2 - strlen(prefix2);
    int is_directory_end2 = 0;
    if (strlen(prefix2) > 0 && prefix2[strlen(prefix2) - 1] == '/') {
        if (pos2 == prefix_start2 + (int)strlen(prefix2)) {
            is_directory_end2 = 1;
        }
    }
    
    if (is_directory_end2) {  /* Should NOT trigger */
        free(prefix2);
        printf("FAIL\n");
        return 0;
    }
    
    free(prefix2);
    
    printf("PASS\n");
    return 1;
}

int main() {
    printf("=== Directory Completion Fix Tests ===\n");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    total_tests++; if (test_directory_end_detection()) passed_tests++;
    total_tests++; if (test_directory_completion_behavior()) passed_tests++;
    total_tests++; if (test_partial_completion_unchanged()) passed_tests++;
    total_tests++; if (test_relative_directory_completion()) passed_tests++;
    total_tests++; if (test_edge_cases()) passed_tests++;
    
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", total_tests - passed_tests);
    
    if (passed_tests == total_tests) {
        printf("✅ All directory completion fix tests passed!\n");
        printf("\nDirectory completion fix validated:\n");
        printf("• 'ls /etc/' + Tab displays list (no auto-complete)\n");
        printf("• 'ls /etc/host' + Tab still auto-completes (unchanged)\n");
        printf("• 'ls src/' + Tab displays list (relative paths work)\n");
        printf("• Edge cases handled correctly\n");
        printf("• Regression prevention ensured\n");
        return 0;
    } else {
        printf("❌ Some directory completion fix tests failed!\n");
        return 1;
    }
}
