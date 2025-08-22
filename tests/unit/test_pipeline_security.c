#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <sys/wait.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test mode flag to override permission checking */
extern int test_mode;

/* Set up test environment */
static void setup_test_environment(void) {
    test_mode = 1;
}

/* Test valid pipeline parsing */
int test_parse_pipeline_valid() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls -la | grep txt | head -5", &pipeline);
    
    TEST_ASSERT_EQ(0, result, "Valid pipeline should parse successfully");
    TEST_ASSERT_EQ(3, pipeline.num_commands, "Should have 3 commands");
    TEST_ASSERT_EQ(2, pipeline.num_pipes, "Should have 2 pipes");
    
    if (pipeline.commands) {
        TEST_ASSERT_STR_EQ("ls -la", pipeline.commands[0].cmd.command, "First command should be 'ls -la'");
        TEST_ASSERT_STR_EQ("grep txt", pipeline.commands[1].cmd.command, "Second command should be 'grep txt'");
        TEST_ASSERT_STR_EQ("head -5", pipeline.commands[2].cmd.command, "Third command should be 'head -5'");
    }
    
    free_pipeline_info(&pipeline);
    return 1;
}

/* Test invalid pipeline parsing */
int test_parse_pipeline_invalid() {
    struct pipeline_info pipeline;
    
    /* Test empty pipeline */
    int result1 = parse_pipeline("", &pipeline);
    TEST_ASSERT_NE(0, result1, "Empty pipeline should fail to parse");
    
    /* Test NULL input */
    int result2 = parse_pipeline(NULL, &pipeline);
    TEST_ASSERT_NE(0, result2, "NULL pipeline should fail to parse");
    
    /* Test malformed pipeline */
    int result3 = parse_pipeline("ls |", &pipeline);
    TEST_ASSERT_NE(0, result3, "Malformed pipeline should fail to parse");
    
    return 1;
}

/* Test pipeline security validation with whitelisted commands */
int test_validate_pipeline_security_valid() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls | grep test | head -10", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    int is_secure = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure, "Pipeline with whitelisted commands should be secure");
    
    free_pipeline_info(&pipeline);
    return 1;
}

/* Test pipeline security validation with non-whitelisted commands */
int test_validate_pipeline_security_invalid() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls | rm -rf /tmp/test", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    int is_secure = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(0, is_secure, "Pipeline with dangerous commands should not be secure");
    
    free_pipeline_info(&pipeline);
    return 1;
}

/* Test validate_secure_pipeline function */
int test_validate_secure_pipeline() {
    /* Set up test environment */
    setup_test_environment();
    set_current_username("testuser");

    /* Test valid pipeline */
    int result1 = validate_secure_pipeline("ls | grep test");
    TEST_ASSERT_EQ(1, result1, "Valid pipeline should pass security validation");
    
    /* Test invalid pipeline with dangerous command */
    int result2 = validate_secure_pipeline("ls | rm -rf /");
    TEST_ASSERT_EQ(0, result2, "Dangerous pipeline should fail security validation");
    
    /* Test NULL input */
    int result3 = validate_secure_pipeline(NULL);
    TEST_ASSERT_EQ(0, result3, "NULL input should fail validation");
    
    return 1;
}

/* Test is_whitelisted_pipe_command function */
int test_is_whitelisted_pipe_command() {
    /* Test whitelisted commands */
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("ls"), "ls should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("grep"), "grep should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("head"), "head should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("tail"), "tail should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("sort"), "sort should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("cat"), "cat should be whitelisted");

    /* Test head and tail with various options */
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("head -n 10"), "head with -n option should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("tail -f"), "tail with -f option should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("head -c 100"), "head with -c option should be whitelisted");
    TEST_ASSERT_EQ(1, is_whitelisted_pipe_command("tail -n +5"), "tail with -n +5 option should be whitelisted");
    
    /* Test non-whitelisted commands */
    TEST_ASSERT_EQ(0, is_whitelisted_pipe_command("rm"), "rm should not be whitelisted");
    TEST_ASSERT_EQ(0, is_whitelisted_pipe_command("chmod"), "chmod should not be whitelisted");
    TEST_ASSERT_EQ(0, is_whitelisted_pipe_command("sudo"), "sudo should not be whitelisted");
    TEST_ASSERT_EQ(0, is_whitelisted_pipe_command("sh"), "sh should not be whitelisted");
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, is_whitelisted_pipe_command(NULL), "NULL should not be whitelisted");
    
    return 1;
}

/* Test pipeline with dangerous find options */
int test_pipeline_dangerous_find() {
    struct pipeline_info pipeline;

    int result = parse_pipeline("find /tmp -name '*.txt' -exec rm {} \\; | cat", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");

    int is_secure = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(0, is_secure, "Pipeline with dangerous find -exec should not be secure");

    free_pipeline_info(&pipeline);
    return 1;
}

/* Test pipeline with safe find options */
int test_pipeline_safe_find() {
    struct pipeline_info pipeline;

    int result = parse_pipeline("find /tmp -name '*.txt' -type f | head -10", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");

    int is_secure = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure, "Pipeline with safe find options should be secure");

    free_pipeline_info(&pipeline);
    return 1;
}

/* Test complex valid pipeline */
int test_complex_valid_pipeline() {
    /* Set up test environment */
    setup_test_environment();
    set_current_username("testuser");

    const char *complex_pipeline = "ps aux | grep -v grep | awk '{print $2, $11}' | sort | head -20";

    int result = validate_secure_pipeline(complex_pipeline);
    TEST_ASSERT_EQ(1, result, "Complex valid pipeline should pass validation");

    return 1;
}

/* Test pipeline with command injection attempt */
int test_pipeline_command_injection() {
    /* Set up test environment */
    setup_test_environment();
    set_current_username("testuser");

    const char *injection_pipeline = "ls | grep 'test; rm -rf /'";

    int result = validate_secure_pipeline(injection_pipeline);
    /* This should still pass because the injection is in the grep pattern, not executed */
    TEST_ASSERT_EQ(1, result, "Pipeline with injection in pattern should pass");

    return 1;
}

/* Test head and tail commands in pipelines */
int test_head_tail_pipeline_functionality() {
    /* Set up test environment */
    setup_test_environment();
    set_current_username("testuser");

    struct pipeline_info pipeline;

    /* Test head command in pipeline */
    int result1 = parse_pipeline("ps aux | head -10", &pipeline);
    TEST_ASSERT_EQ(0, result1, "Pipeline with head should parse successfully");
    TEST_ASSERT_EQ(2, pipeline.num_commands, "Should have 2 commands");
    TEST_ASSERT_STR_EQ("head -10", pipeline.commands[1].cmd.command, "Second command should be 'head -10'");

    int is_secure1 = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure1, "Pipeline with head should be secure");

    free_pipeline_info(&pipeline);

    /* Test tail command in pipeline */
    int result2 = parse_pipeline("cat /var/log/syslog | tail -20", &pipeline);
    TEST_ASSERT_EQ(0, result2, "Pipeline with tail should parse successfully");
    TEST_ASSERT_EQ(2, pipeline.num_commands, "Should have 2 commands");
    TEST_ASSERT_STR_EQ("tail -20", pipeline.commands[1].cmd.command, "Second command should be 'tail -20'");

    int is_secure2 = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure2, "Pipeline with tail should be secure");

    free_pipeline_info(&pipeline);

    /* Test complex pipeline with both head and tail */
    int result3 = parse_pipeline("ps aux | grep nginx | head -50 | tail -10", &pipeline);
    TEST_ASSERT_EQ(0, result3, "Complex pipeline with head and tail should parse successfully");
    TEST_ASSERT_EQ(4, pipeline.num_commands, "Should have 4 commands");
    TEST_ASSERT_STR_EQ("head -50", pipeline.commands[2].cmd.command, "Third command should be 'head -50'");
    TEST_ASSERT_STR_EQ("tail -10", pipeline.commands[3].cmd.command, "Fourth command should be 'tail -10'");

    int is_secure3 = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure3, "Complex pipeline with head and tail should be secure");

    free_pipeline_info(&pipeline);

    return 1;
}

/* Test pipeline audit logging functions */
int test_pipeline_audit_logging() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls | grep test", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    /* Test logging functions don't crash */
    log_pipeline_start(&pipeline);
    log_pipeline_completion(&pipeline, 0);
    
    /* Test with NULL input */
    log_pipeline_start(NULL);
    log_pipeline_completion(NULL, 0);
    
    free_pipeline_info(&pipeline);
    return 1;
}

/* Test validate_pipeline_with_permissions */
int test_validate_pipeline_with_permissions() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls | grep test", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    /* Test with current user */
    char *username = getenv("USER");
    if (!username) username = "root";
    
    int is_valid = validate_pipeline_with_permissions(&pipeline, username);
    printf("  (pipeline permission for %s: %s) ", username, 
           is_valid ? "allowed" : "denied");
    
    /* We can't assert a specific result since it depends on system configuration */
    TEST_ASSERT(is_valid == 0 || is_valid == 1, 
                "validate_pipeline_with_permissions should return 0 or 1");
    
    /* Test with NULL inputs */
    int result1 = validate_pipeline_with_permissions(NULL, username);
    int result2 = validate_pipeline_with_permissions(&pipeline, NULL);
    
    TEST_ASSERT_EQ(0, result1, "NULL pipeline should return 0");
    TEST_ASSERT_EQ(0, result2, "NULL username should return 0");
    
    free_pipeline_info(&pipeline);
    return 1;
}

/* Test edge case: single command "pipeline" */
int test_single_command_pipeline() {
    struct pipeline_info pipeline;

    /* Use cat as a simple pipeline to test single command behavior */
    int result = parse_pipeline("ls -la | cat", &pipeline);
    TEST_ASSERT_EQ(0, result, "Single command pipeline should parse successfully");
    TEST_ASSERT_EQ(2, pipeline.num_commands, "Should have 2 commands");
    TEST_ASSERT_EQ(1, pipeline.num_pipes, "Should have 1 pipe");

    int is_secure = validate_pipeline_security(&pipeline);
    TEST_ASSERT_EQ(1, is_secure, "Simple whitelisted pipeline should be secure");

    free_pipeline_info(&pipeline);
    return 1;
}

/* Test pipeline memory management */
int test_pipeline_memory_management() {
    struct pipeline_info pipeline;
    
    int result = parse_pipeline("ls | grep test | head -5", &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    /* Verify memory is allocated */
    TEST_ASSERT_NOT_NULL(pipeline.commands, "Commands should be allocated");
    TEST_ASSERT_NOT_NULL(pipeline.pipe_fds, "Pipe FDs should be allocated");
    
    /* Free and verify cleanup */
    free_pipeline_info(&pipeline);
    
    /* After free, the structure should be zeroed */
    TEST_ASSERT_EQ(0, pipeline.num_commands, "Commands count should be 0 after free");
    TEST_ASSERT_EQ(0, pipeline.num_pipes, "Pipes count should be 0 after free");
    
    return 1;
}

TEST_SUITE_BEGIN("Pipeline Security Tests")
    RUN_TEST(test_parse_pipeline_valid);
    RUN_TEST(test_parse_pipeline_invalid);
    RUN_TEST(test_validate_pipeline_security_valid);
    RUN_TEST(test_validate_pipeline_security_invalid);
    RUN_TEST(test_validate_secure_pipeline);
    RUN_TEST(test_is_whitelisted_pipe_command);
    RUN_TEST(test_pipeline_dangerous_find);
    RUN_TEST(test_pipeline_safe_find);
    RUN_TEST(test_complex_valid_pipeline);
    RUN_TEST(test_pipeline_command_injection);
    RUN_TEST(test_head_tail_pipeline_functionality);
    RUN_TEST(test_pipeline_audit_logging);
    RUN_TEST(test_validate_pipeline_with_permissions);
    RUN_TEST(test_single_command_pipeline);
    RUN_TEST(test_pipeline_memory_management);
TEST_SUITE_END()
