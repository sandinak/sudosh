#include "test_framework.h"
#include "../sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test trim_whitespace function */
int test_trim_whitespace() {
    char test1[] = "  hello world  ";
    char *result1 = trim_whitespace(test1);
    TEST_ASSERT_STR_EQ("hello world", result1, "trim_whitespace should remove leading and trailing spaces");
    
    char test2[] = "\t\nhello\t\n";
    char *result2 = trim_whitespace(test2);
    TEST_ASSERT_STR_EQ("hello", result2, "trim_whitespace should remove tabs and newlines");
    
    char test3[] = "no_spaces";
    char *result3 = trim_whitespace(test3);
    TEST_ASSERT_STR_EQ("no_spaces", result3, "trim_whitespace should not modify strings without whitespace");
    
    char test4[] = "   ";
    char *result4 = trim_whitespace(test4);
    TEST_ASSERT_STR_EQ("", result4, "trim_whitespace should return empty string for whitespace-only input");
    
    return 1;
}

/* Test is_empty_command function */
int test_is_empty_command() {
    TEST_ASSERT_EQ(1, is_empty_command(""), "empty string should be considered empty command");
    TEST_ASSERT_EQ(1, is_empty_command("   "), "whitespace-only string should be considered empty command");
    TEST_ASSERT_EQ(1, is_empty_command("\t\n"), "tabs and newlines should be considered empty command");
    TEST_ASSERT_EQ(0, is_empty_command("ls"), "actual command should not be considered empty");
    TEST_ASSERT_EQ(0, is_empty_command("  ls  "), "command with whitespace should not be considered empty");
    TEST_ASSERT_EQ(1, is_empty_command(NULL), "NULL should be considered empty command");
    
    return 1;
}

/* Test is_whitespace_only function */
int test_is_whitespace_only() {
    TEST_ASSERT_EQ(1, is_whitespace_only(""), "empty string should be whitespace only");
    TEST_ASSERT_EQ(1, is_whitespace_only("   "), "spaces should be whitespace only");
    TEST_ASSERT_EQ(1, is_whitespace_only("\t\n\r"), "tabs and newlines should be whitespace only");
    TEST_ASSERT_EQ(0, is_whitespace_only("hello"), "text should not be whitespace only");
    TEST_ASSERT_EQ(0, is_whitespace_only("  hello  "), "text with whitespace should not be whitespace only");
    TEST_ASSERT_EQ(1, is_whitespace_only(NULL), "NULL should be considered whitespace only");
    
    return 1;
}

/* Test safe_strdup function */
int test_safe_strdup() {
    char *result1 = safe_strdup("hello");
    TEST_ASSERT_NOT_NULL(result1, "safe_strdup should return non-NULL for valid string");
    TEST_ASSERT_STR_EQ("hello", result1, "safe_strdup should copy string correctly");
    free(result1);
    
    char *result2 = safe_strdup("");
    TEST_ASSERT_NOT_NULL(result2, "safe_strdup should return non-NULL for empty string");
    TEST_ASSERT_STR_EQ("", result2, "safe_strdup should copy empty string correctly");
    free(result2);
    
    char *result3 = safe_strdup(NULL);
    TEST_ASSERT_NULL(result3, "safe_strdup should return NULL for NULL input");
    
    return 1;
}

/* Test get_current_username function */
int test_get_current_username() {
    char *username = get_current_username();
    TEST_ASSERT_NOT_NULL(username, "get_current_username should return non-NULL");
    
    /* Username should be non-empty */
    TEST_ASSERT(strlen(username) > 0, "username should be non-empty");
    
    /* Username should not contain invalid characters */
    for (char *p = username; *p; p++) {
        TEST_ASSERT(*p != '\0' && *p != '\n', "username should not contain null bytes or newlines");
    }
    
    free(username);
    return 1;
}

/* Test user_info functions */
int test_user_info() {
    /* Test with current user */
    char *username = get_current_username();
    TEST_ASSERT_NOT_NULL(username, "get_current_username should succeed");
    
    struct user_info *user = get_user_info(username);
    TEST_ASSERT_NOT_NULL(user, "get_user_info should return non-NULL for valid user");
    
    if (user) {
        TEST_ASSERT_NOT_NULL(user->username, "user_info should have username");
        TEST_ASSERT_NOT_NULL(user->home_dir, "user_info should have home_dir");
        TEST_ASSERT_NOT_NULL(user->shell, "user_info should have shell");
        TEST_ASSERT_STR_EQ(username, user->username, "user_info username should match input");
        
        free_user_info(user);
    }
    
    /* Test with invalid user */
    struct user_info *invalid_user = get_user_info("nonexistent_user_12345");
    TEST_ASSERT_NULL(invalid_user, "get_user_info should return NULL for invalid user");
    
    /* Test with NULL input */
    struct user_info *null_user = get_user_info(NULL);
    TEST_ASSERT_NULL(null_user, "get_user_info should return NULL for NULL input");
    
    free(username);
    return 1;
}

/* Test command parsing */
int test_parse_command() {
    struct command_info cmd;
    
    /* Test simple command */
    int result1 = parse_command("ls -la", &cmd);
    TEST_ASSERT_EQ(0, result1, "parse_command should succeed for valid command");
    TEST_ASSERT_EQ(2, cmd.argc, "parsed command should have correct argc");
    TEST_ASSERT_STR_EQ("ls", cmd.argv[0], "first argument should be command");
    TEST_ASSERT_STR_EQ("-la", cmd.argv[1], "second argument should be option");
    TEST_ASSERT_NULL(cmd.argv[2], "argv should be null-terminated");
    free_command_info(&cmd);
    
    /* Test command with multiple arguments */
    int result2 = parse_command("cp file1 file2 /dest", &cmd);
    TEST_ASSERT_EQ(0, result2, "parse_command should succeed for multi-arg command");
    TEST_ASSERT_EQ(4, cmd.argc, "parsed command should have correct argc");
    TEST_ASSERT_STR_EQ("cp", cmd.argv[0], "command should be parsed correctly");
    TEST_ASSERT_STR_EQ("/dest", cmd.argv[3], "last argument should be parsed correctly");
    free_command_info(&cmd);
    
    /* Test empty command */
    int result3 = parse_command("", &cmd);
    TEST_ASSERT_EQ(0, result3, "parse_command should handle empty command");
    TEST_ASSERT_EQ(0, cmd.argc, "empty command should have argc 0");
    free_command_info(&cmd);
    
    /* Test NULL input */
    int result4 = parse_command(NULL, &cmd);
    TEST_ASSERT_EQ(-1, result4, "parse_command should fail for NULL input");
    
    return 1;
}

/* Test command validation */
int test_validate_command() {
    TEST_ASSERT_EQ(1, validate_command("ls -la"), "valid command should pass validation");
    TEST_ASSERT_EQ(1, validate_command("systemctl status nginx"), "complex valid command should pass");
    
    /* Test null byte injection - note: C strings terminate at first null byte,
     * so this test actually just tests "ls" which is valid. Real null byte
     * injection would need to be handled at a different level. */
    char null_cmd[] = "ls\0rm -rf /";
    /* This will actually pass validation since strlen() stops at the null byte */
    int null_result = validate_command(null_cmd);
    printf("  (null byte test result: %d) ", null_result);
    
    /* Test path traversal */
    TEST_ASSERT_EQ(0, validate_command("cat ../../../etc/passwd"), "path traversal should fail validation");
    TEST_ASSERT_EQ(0, validate_command("ls ..\\windows"), "windows path traversal should fail validation");
    
    /* Test very long command */
    char *long_cmd = malloc(MAX_COMMAND_LENGTH + 100);
    memset(long_cmd, 'a', MAX_COMMAND_LENGTH + 50);
    long_cmd[MAX_COMMAND_LENGTH + 50] = '\0';
    TEST_ASSERT_EQ(0, validate_command(long_cmd), "overly long command should fail validation");
    free(long_cmd);
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, validate_command(NULL), "NULL command should fail validation");
    
    return 1;
}

TEST_SUITE_BEGIN("Unit Tests - Utilities")
    RUN_TEST(test_trim_whitespace);
    RUN_TEST(test_is_empty_command);
    RUN_TEST(test_is_whitespace_only);
    RUN_TEST(test_safe_strdup);
    RUN_TEST(test_get_current_username);
    RUN_TEST(test_user_info);
    RUN_TEST(test_parse_command);
    RUN_TEST(test_validate_command);
TEST_SUITE_END()
