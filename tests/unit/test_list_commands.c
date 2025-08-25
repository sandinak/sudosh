/**
 * test_list_commands.c - Test the -l/--list functionality
 *
 * Tests the -l/--list option that displays available commands from sudoers
 */

#include "test_framework.h"
#include "../src/sudosh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

/**
 * Test the list_available_commands function
 */
void test_list_available_commands(void) {
    printf("Testing list_available_commands function...\n");
    
    /* Get current username */
    struct passwd *pwd = getpwuid(getuid());
    if (!pwd) {
        printf("FAIL: Could not get current user\n");
        return;
    }
    
    printf("Testing with user: %s\n", pwd->pw_name);
    
    /* Redirect stdout to capture output */
    FILE *original_stdout = stdout;
    FILE *temp_file = tmpfile();
    if (!temp_file) {
        printf("FAIL: Could not create temporary file\n");
        return;
    }
    
    stdout = temp_file;
    
    /* Call the function */
    list_available_commands(pwd->pw_name);
    
    /* Restore stdout */
    stdout = original_stdout;
    
    /* Read the output */
    rewind(temp_file);
    char buffer[1024];
    int found_output = 0;
    
    while (fgets(buffer, sizeof(buffer), temp_file)) {
        if (strstr(buffer, "Sudo privileges for") ||
            strstr(buffer, "Defaults Configuration") ||
            strstr(buffer, "Direct Sudoers Rules") ||
            strstr(buffer, "Group-Based Privileges") ||
            strstr(buffer, "has sudo privileges")) {
            found_output = 1;
            break;
        }
    }
    
    fclose(temp_file);
    
    if (found_output) {
        printf("PASS: list_available_commands produced expected output\n");
    } else {
        printf("FAIL: list_available_commands did not produce expected output\n");
    }
}

/**
 * Test with NULL username
 */
void test_list_commands_null_user(void) {
    printf("Testing list_available_commands with NULL user...\n");
    
    /* Redirect stdout to capture output */
    FILE *original_stdout = stdout;
    FILE *temp_file = tmpfile();
    if (!temp_file) {
        printf("FAIL: Could not create temporary file\n");
        return;
    }
    
    stdout = temp_file;
    
    /* Call the function with NULL */
    list_available_commands(NULL);
    
    /* Restore stdout */
    stdout = original_stdout;
    
    /* Read the output */
    rewind(temp_file);
    char buffer[1024];
    int found_error = 0;
    
    while (fgets(buffer, sizeof(buffer), temp_file)) {
        if (strstr(buffer, "Error: No username provided")) {
            found_error = 1;
            break;
        }
    }
    
    fclose(temp_file);
    
    if (found_error) {
        printf("PASS: list_available_commands handled NULL user correctly\n");
    } else {
        printf("FAIL: list_available_commands did not handle NULL user correctly\n");
    }
}

int main(void) {
    printf("=== Testing -l/--list functionality ===\n\n");
    
    test_list_available_commands();
    test_list_commands_null_user();
    
    printf("\n=== Test completed ===\n");
    return 0;
}
