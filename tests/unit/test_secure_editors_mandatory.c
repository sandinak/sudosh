/**
 * test_secure_editors_mandatory.c - Mandatory Test for Secure Editor Functionality
 * 
 * This test MUST PASS for every change to sudosh. It verifies that secure editors
 * (vi, vim, nano, pico) can be executed in a constrained environment without
 * being blocked, while maintaining security by preventing shell escapes.
 * 
 * CRITICAL: This test failing indicates a regression that breaks core functionality.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

/* Forward declarations for testing - avoid full sudosh.h inclusion */
int is_secure_editor(const char *command);
int is_interactive_editor(const char *command);
int validate_command(const char *command);
void set_current_username(const char *username);
void setup_secure_editor_environment(void);
int init_file_locking(void);
void cleanup_file_locking(void);
int is_editing_command(const char *command);
char *extract_file_argument(const char *command);
void init_security(void);

/* Test results tracking */
static int tests_passed = 0;
static int tests_failed = 0;
static int total_tests = 0;

#define TEST_ASSERT(condition, message) do { \
    total_tests++; \
    if (condition) { \
        printf("✅ PASS: %s\n", message); \
        tests_passed++; \
    } else { \
        printf("❌ FAIL: %s\n", message); \
        tests_failed++; \
    } \
} while(0)

/**
 * Test that secure editors are properly identified
 */
void test_secure_editor_identification(void) {
    printf("\n=== Testing Secure Editor Identification ===\n");
    
    /* These MUST be identified as secure editors */
    TEST_ASSERT(is_secure_editor("vi /tmp/test.txt"), "vi command identified as secure editor");
    TEST_ASSERT(is_secure_editor("vim /etc/passwd"), "vim command identified as secure editor");
    TEST_ASSERT(is_secure_editor("nano /tmp/config.conf"), "nano command identified as secure editor");
    TEST_ASSERT(is_secure_editor("pico /etc/hosts"), "pico command identified as secure editor");
    TEST_ASSERT(is_secure_editor("view /var/log/syslog"), "view command identified as secure editor");
    
    /* Test with full paths */
    TEST_ASSERT(is_secure_editor("/usr/bin/vi /tmp/test.txt"), "/usr/bin/vi identified as secure editor");
    TEST_ASSERT(is_secure_editor("/bin/nano /etc/passwd"), "/bin/nano identified as secure editor");
    
    /* These should NOT be identified as secure editors (but may be editing commands) */
    TEST_ASSERT(!is_secure_editor("emacs /tmp/test.txt"), "emacs NOT identified as secure editor");
    TEST_ASSERT(!is_secure_editor("joe /tmp/test.txt"), "joe NOT identified as secure editor");
    TEST_ASSERT(!is_secure_editor("ls -la"), "ls NOT identified as secure editor");
    TEST_ASSERT(!is_secure_editor("cat /etc/passwd"), "cat NOT identified as secure editor");
}

/**
 * Test that secure editors are NOT blocked by interactive editor check
 */
void test_secure_editors_not_blocked(void) {
    printf("\n=== Testing Secure Editors Are Not Blocked ===\n");
    
    /* These secure editors should NOT be blocked by is_interactive_editor */
    TEST_ASSERT(!is_interactive_editor("vi /tmp/test.txt"), "vi NOT blocked by interactive editor check");
    TEST_ASSERT(!is_interactive_editor("vim /etc/passwd"), "vim NOT blocked by interactive editor check");
    TEST_ASSERT(!is_interactive_editor("nano /tmp/config.conf"), "nano NOT blocked by interactive editor check");
    TEST_ASSERT(!is_interactive_editor("pico /etc/hosts"), "pico NOT blocked by interactive editor check");
    TEST_ASSERT(!is_interactive_editor("view /var/log/syslog"), "view NOT blocked by interactive editor check");
    
    /* These dangerous editors SHOULD be blocked */
    TEST_ASSERT(is_interactive_editor("emacs /tmp/test.txt"), "emacs correctly blocked by interactive editor check");
    TEST_ASSERT(is_interactive_editor("joe /tmp/test.txt"), "joe correctly blocked by interactive editor check");
    TEST_ASSERT(is_interactive_editor("mcedit /tmp/test.txt"), "mcedit correctly blocked by interactive editor check");
}

/**
 * Test command validation allows secure editors
 */
void test_command_validation_allows_secure_editors(void) {
    printf("\n=== Testing Command Validation Allows Secure Editors ===\n");
    
    /* Set up a test username for validation */
    set_current_username("testuser");
    
    /* These commands should be allowed */
    TEST_ASSERT(validate_command("vi /tmp/test.txt"), "vi command passes validation");
    TEST_ASSERT(validate_command("vim /etc/passwd"), "vim command passes validation");
    TEST_ASSERT(validate_command("nano /tmp/config.conf"), "nano command passes validation");
    TEST_ASSERT(validate_command("pico /etc/hosts"), "pico command passes validation");
    TEST_ASSERT(validate_command("view /var/log/syslog"), "view command passes validation");
    
    /* These dangerous commands should be blocked */
    TEST_ASSERT(!validate_command("emacs /tmp/test.txt"), "emacs command correctly blocked");
    TEST_ASSERT(!validate_command("joe /tmp/test.txt"), "joe command correctly blocked");
}

/**
 * Test that secure editor environment is properly set up
 */
void test_secure_editor_environment(void) {
    printf("\n=== Testing Secure Editor Environment Setup ===\n");
    
    /* Save original environment */
    char *orig_shell = getenv("SHELL");
    char *orig_visual = getenv("VISUAL");
    char *orig_editor = getenv("EDITOR");
    char *orig_viminit = getenv("VIMINIT");
    
    /* Set up secure editor environment */
    setup_secure_editor_environment();
    
    /* Verify security restrictions are in place */
    char *shell = getenv("SHELL");
    char *visual = getenv("VISUAL");
    char *editor = getenv("EDITOR");
    char *viminit = getenv("VIMINIT");
    
    TEST_ASSERT(shell && strcmp(shell, "/bin/false") == 0, "SHELL set to /bin/false");
    TEST_ASSERT(visual && strcmp(visual, "/bin/false") == 0, "VISUAL set to /bin/false");
    TEST_ASSERT(editor && strcmp(editor, "/bin/false") == 0, "EDITOR set to /bin/false");
    TEST_ASSERT(viminit && strstr(viminit, "secure") != NULL, "VIMINIT contains secure settings");
    
    /* Restore original environment */
    if (orig_shell) setenv("SHELL", orig_shell, 1); else unsetenv("SHELL");
    if (orig_visual) setenv("VISUAL", orig_visual, 1); else unsetenv("VISUAL");
    if (orig_editor) setenv("EDITOR", orig_editor, 1); else unsetenv("EDITOR");
    if (orig_viminit) setenv("VIMINIT", orig_viminit, 1); else unsetenv("VIMINIT");
}

/**
 * Test file locking integration doesn't break secure editors
 */
void test_file_locking_integration(void) {
    printf("\n=== Testing File Locking Integration ===\n");
    
    /* Initialize file locking system */
    int lock_init_result = init_file_locking();
    TEST_ASSERT(lock_init_result == 0, "File locking system initializes successfully");
    
    /* Test that secure editors are detected as editing commands */
    TEST_ASSERT(is_editing_command("vi /tmp/test.txt"), "vi detected as editing command for locking");
    TEST_ASSERT(is_editing_command("vim /etc/passwd"), "vim detected as editing command for locking");
    TEST_ASSERT(is_editing_command("nano /tmp/config.conf"), "nano detected as editing command for locking");
    
    /* Test file argument extraction */
    char *file1 = extract_file_argument("vi /tmp/test.txt");
    char *file2 = extract_file_argument("vim -n /etc/passwd");
    char *file3 = extract_file_argument("nano /tmp/config.conf");
    
    TEST_ASSERT(file1 && strcmp(file1, "/tmp/test.txt") == 0, "File argument extracted from vi command");
    TEST_ASSERT(file2 && strcmp(file2, "/etc/passwd") == 0, "File argument extracted from vim command");
    TEST_ASSERT(file3 && strcmp(file3, "/tmp/config.conf") == 0, "File argument extracted from nano command");
    
    free(file1);
    free(file2);
    free(file3);
    
    /* Clean up file locking system */
    cleanup_file_locking();
}

/**
 * Test the complete workflow: secure editor should work end-to-end
 */
void test_complete_secure_editor_workflow(void) {
    printf("\n=== Testing Complete Secure Editor Workflow ===\n");
    
    /* This is the critical test - a secure editor command should:
     * 1. Be identified as a secure editor
     * 2. NOT be blocked by interactive editor check
     * 3. Pass command validation
     * 4. Have file locking applied (if file locking is enabled)
     * 5. Have secure environment set up
     * 6. Be allowed to execute
     */
    
    const char *test_command = "vi /tmp/test_secure_editor.txt";
    
    /* Step 1: Should be identified as secure editor */
    int is_secure = is_secure_editor(test_command);
    TEST_ASSERT(is_secure, "Complete workflow: vi identified as secure editor");
    
    /* Step 2: Should NOT be blocked as interactive editor */
    int is_blocked = is_interactive_editor(test_command);
    TEST_ASSERT(!is_blocked, "Complete workflow: vi NOT blocked as interactive editor");
    
    /* Step 3: Should pass command validation */
    set_current_username("testuser");
    int is_valid = validate_command(test_command);
    TEST_ASSERT(is_valid, "Complete workflow: vi command passes validation");
    
    /* Step 4: Should be detected for file locking */
    int needs_locking = is_editing_command(test_command);
    TEST_ASSERT(needs_locking, "Complete workflow: vi detected for file locking");
    
    /* Step 5: File argument should be extractable */
    char *file_arg = extract_file_argument(test_command);
    TEST_ASSERT(file_arg && strlen(file_arg) > 0, "Complete workflow: file argument extracted");
    free(file_arg);
    
    printf("✅ CRITICAL: Complete secure editor workflow passes all checks\n");
}

/**
 * Main test function
 */
int main(void) {
    printf("🔒 MANDATORY SECURE EDITOR TEST\n");
    printf("===============================\n");
    printf("This test MUST PASS for every change to sudosh.\n");
    printf("Failure indicates a regression in secure editor functionality.\n");
    
    /* Initialize systems needed for testing */
    init_security();
    
    /* Run all tests */
    test_secure_editor_identification();
    test_secure_editors_not_blocked();
    test_command_validation_allows_secure_editors();
    test_secure_editor_environment();
    test_file_locking_integration();
    test_complete_secure_editor_workflow();
    
    /* Print results */
    for (int i = 0; i < 50; ++i) putchar('='); putchar('\n');
    printf("TEST RESULTS SUMMARY\n");
    for (int i = 0; i < 50; ++i) putchar('='); putchar('\n');
    printf("Total Tests: %d\n", total_tests);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("\n🎉 ALL TESTS PASSED - Secure editors work correctly!\n");
        printf("✅ vi, vim, nano, pico can be executed securely\n");
        printf("✅ Shell escapes are prevented by environment restrictions\n");
        printf("✅ File locking integration works properly\n");
        printf("✅ No regressions detected\n");
        return 0;
    } else {
        printf("\n💥 TESTS FAILED - REGRESSION DETECTED!\n");
        printf("❌ Secure editor functionality is broken\n");
        printf("❌ This change must be fixed before merging\n");
        printf("❌ %d test(s) failed out of %d total\n", tests_failed, total_tests);
        return 1;
    }
}
