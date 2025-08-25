#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test shell operator detection */
int test_contains_shell_operators() {
    /* Test redirection operators */
    TEST_ASSERT_EQ(1, contains_shell_operators("echo test > file.txt"), 
                   "Should detect > operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("echo test >> file.txt"), 
                   "Should detect >> operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("cat < input.txt"), 
                   "Should detect < operator");
    
    /* Test pipe operators */
    TEST_ASSERT_EQ(1, contains_shell_operators("ls | grep test"), 
                   "Should detect | operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("cat file | head -5 | tail -2"), 
                   "Should detect multiple | operators");
    
    /* Test command chaining operators */
    TEST_ASSERT_EQ(1, contains_shell_operators("ls; pwd"), 
                   "Should detect ; operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("ls && pwd"), 
                   "Should detect && operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("ls || pwd"), 
                   "Should detect || operator");
    TEST_ASSERT_EQ(1, contains_shell_operators("ls & pwd"), 
                   "Should detect & operator");
    
    /* Test command substitution */
    TEST_ASSERT_EQ(1, contains_shell_operators("echo `whoami`"), 
                   "Should detect backtick substitution");
    TEST_ASSERT_EQ(1, contains_shell_operators("echo $(whoami)"), 
                   "Should detect $() substitution");
    
    /* Test commands without operators */
    TEST_ASSERT_EQ(0, contains_shell_operators("ls -la"), 
                   "Should not detect operators in simple command");
    TEST_ASSERT_EQ(0, contains_shell_operators("grep pattern file.txt"), 
                   "Should not detect operators in grep command");
    
    /* Test quoted operators (should not be detected) */
    TEST_ASSERT_EQ(0, contains_shell_operators("echo 'test > file'"), 
                   "Should not detect quoted > operator");
    TEST_ASSERT_EQ(0, contains_shell_operators("echo \"ls | grep\""), 
                   "Should not detect quoted | operator");
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, contains_shell_operators(NULL), 
                   "Should handle NULL input");
    
    return 1;
}

/* Test redirection parsing */
int test_parse_command_with_redirection() {
    struct command_info cmd;
    
    /* Test output redirection */
    int result = parse_command_with_redirection("echo test > /tmp/output.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse output redirection successfully");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd.redirect_type, "Should set correct redirect type");
    TEST_ASSERT_STR_EQ("/tmp/output.txt", cmd.redirect_file, "Should set correct redirect file");
    TEST_ASSERT_STR_EQ("echo", cmd.argv[0], "Should parse command correctly");
    TEST_ASSERT_STR_EQ("test", cmd.argv[1], "Should parse argument correctly");
    free_command_info(&cmd);
    
    /* Test append redirection */
    result = parse_command_with_redirection("echo test >> /tmp/append.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse append redirection successfully");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT_APPEND, cmd.redirect_type, "Should set correct append type");
    TEST_ASSERT_STR_EQ("/tmp/append.txt", cmd.redirect_file, "Should set correct append file");
    free_command_info(&cmd);
    
    /* Test input redirection */
    result = parse_command_with_redirection("sort < /tmp/input.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse input redirection successfully");
    TEST_ASSERT_EQ(REDIRECT_INPUT, cmd.redirect_type, "Should set correct input type");
    TEST_ASSERT_STR_EQ("/tmp/input.txt", cmd.redirect_file, "Should set correct input file");
    free_command_info(&cmd);
    
    /* Test unsafe redirection (should fail) */
    result = parse_command_with_redirection("echo malicious > /etc/passwd", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject unsafe redirection");
    
    /* Test NULL inputs */
    result = parse_command_with_redirection(NULL, &cmd);
    TEST_ASSERT_NE(0, result, "Should handle NULL input");
    
    result = parse_command_with_redirection("echo test > /tmp/file", NULL);
    TEST_ASSERT_NE(0, result, "Should handle NULL cmd");
    
    return 1;
}

/* Test enhanced parse_command function */
int test_enhanced_parse_command() {
    struct command_info cmd;
    
    /* Test simple command (no operators) */
    int result = parse_command("ls -la", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse simple command");
    TEST_ASSERT_STR_EQ("ls", cmd.argv[0], "Should parse command name");
    TEST_ASSERT_STR_EQ("-la", cmd.argv[1], "Should parse argument");
    TEST_ASSERT_EQ(REDIRECT_NONE, cmd.redirect_type, "Should have no redirection");
    free_command_info(&cmd);
    
    /* Test command with redirection */
    result = parse_command("echo test > /tmp/test.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse command with redirection");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd.redirect_type, "Should detect redirection");
    TEST_ASSERT_STR_EQ("/tmp/test.txt", cmd.redirect_file, "Should set redirect file");
    free_command_info(&cmd);
    
    /* Test command with pipe (should fail in regular parser) */
    result = parse_command("ls | grep test", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject pipe in regular command parser");
    
    /* Test command with dangerous operators */
    result = parse_command("ls; rm file", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject command chaining");
    
    result = parse_command("ls && rm file", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject logical AND");
    
    result = parse_command("ls `whoami`", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject command substitution");
    
    return 1;
}

/* Test tokenize_command_line function */
int test_tokenize_command_line() {
    char **argv = NULL;
    int argc = 0;
    int argv_size = 16;
    
    argv = malloc(argv_size * sizeof(char *));
    TEST_ASSERT_NOT_NULL(argv, "Should allocate argv");
    
    /* Test simple tokenization */
    int result = tokenize_command_line("ls -la /tmp", &argv, &argc, &argv_size);
    TEST_ASSERT_EQ(0, result, "Should tokenize successfully");
    TEST_ASSERT_EQ(3, argc, "Should have 3 arguments");
    TEST_ASSERT_STR_EQ("ls", argv[0], "Should parse command");
    TEST_ASSERT_STR_EQ("-la", argv[1], "Should parse flag");
    TEST_ASSERT_STR_EQ("/tmp", argv[2], "Should parse path");
    
    /* Clean up */
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
    
    /* Test NULL inputs */
    result = tokenize_command_line(NULL, &argv, &argc, &argv_size);
    TEST_ASSERT_NE(0, result, "Should handle NULL input");
    
    return 1;
}

/* Test trim_whitespace_inplace function */
int test_trim_whitespace_inplace() {
    char test_str1[] = "  hello world  ";
    char *result = trim_whitespace_inplace(test_str1);
    TEST_ASSERT_STR_EQ("hello world", result, "Should trim leading and trailing whitespace");
    
    char test_str2[] = "no_whitespace";
    result = trim_whitespace_inplace(test_str2);
    TEST_ASSERT_STR_EQ("no_whitespace", result, "Should handle string without whitespace");
    
    char test_str3[] = "   ";
    result = trim_whitespace_inplace(test_str3);
    TEST_ASSERT_STR_EQ("", result, "Should handle whitespace-only string");
    
    char test_str4[] = "";
    result = trim_whitespace_inplace(test_str4);
    TEST_ASSERT_STR_EQ("", result, "Should handle empty string");
    
    /* Test NULL input */
    result = trim_whitespace_inplace(NULL);
    TEST_ASSERT_NULL(result, "Should handle NULL input");
    
    return 1;
}

/* Test redirection with command execution (integration test) */
int test_redirection_integration() {
    struct command_info cmd;
    
    /* Test that redirection fields are properly initialized */
    int result = parse_command("echo test", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse simple command");
    TEST_ASSERT_EQ(REDIRECT_NONE, cmd.redirect_type, "Should initialize redirect type");
    TEST_ASSERT_NULL(cmd.redirect_file, "Should initialize redirect file");
    TEST_ASSERT_EQ(0, cmd.redirect_append, "Should initialize redirect append");
    free_command_info(&cmd);
    
    /* Test that redirection fields are properly set */
    result = parse_command("echo test > /tmp/integration_test.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse redirection command");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd.redirect_type, "Should set redirect type");
    TEST_ASSERT_NOT_NULL(cmd.redirect_file, "Should set redirect file");
    TEST_ASSERT_STR_EQ("/tmp/integration_test.txt", cmd.redirect_file, "Should set correct file");
    
    /* Test that free_command_info cleans up redirection fields */
    free_command_info(&cmd);
    TEST_ASSERT_EQ(REDIRECT_NONE, cmd.redirect_type, "Should reset redirect type");
    TEST_ASSERT_NULL(cmd.redirect_file, "Should reset redirect file");
    TEST_ASSERT_EQ(0, cmd.redirect_append, "Should reset redirect append");
    
    return 1;
}

/* Test edge cases and error handling */
int test_redirection_edge_cases() {
    struct command_info cmd;

    /* Test malformed redirection */
    int result = parse_command_with_redirection("echo test >", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject redirection without target");

    /* Test redirection with only whitespace target */
    result = parse_command_with_redirection("echo test >   ", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject redirection with whitespace target");

    /* Test multiple redirection operators */
    result = parse_command("echo test > file1 > file2", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject multiple redirection operators");

    /* Test redirection with spaces */
    result = parse_command_with_redirection("echo test   >   /tmp/spaced.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should handle redirection with spaces");
    TEST_ASSERT_STR_EQ("/tmp/spaced.txt", cmd.redirect_file, "Should parse spaced redirection");
    free_command_info(&cmd);

    return 1;
}

/* Test pipeline redirection integration */
int test_pipeline_redirection_integration() {
    /* Test that pipeline commands with redirection are properly parsed */

    /* Test simple pipeline with redirection */
    TEST_ASSERT_EQ(1, is_pipeline_command("cat file | grep pattern > output"),
                   "Should detect pipeline with redirection");

    /* Test complex pipeline with redirection */
    TEST_ASSERT_EQ(1, is_pipeline_command("ps aux | grep bash | head -5 > /tmp/processes"),
                   "Should detect complex pipeline with redirection");

    /* Test pipeline with multiple redirections */
    TEST_ASSERT_EQ(1, is_pipeline_command("cat < input | sort > output"),
                   "Should detect pipeline with input and output redirection");

    /* Parse a pipeline command and verify redirection is handled */
    struct pipeline_info pipeline;
    int result = parse_pipeline("echo test | grep test > /tmp/pipeline_test", &pipeline);
    TEST_ASSERT_EQ(0, result, "Should parse pipeline with redirection");

    if (result == 0) {
        TEST_ASSERT_EQ(2, pipeline.num_commands, "Should have 2 commands in pipeline");

        /* Check the last command has redirection */
        struct command_info *last_cmd = &pipeline.commands[1].cmd;
        TEST_ASSERT_EQ(REDIRECT_OUTPUT, last_cmd->redirect_type, "Last command should have output redirection");
        TEST_ASSERT_STR_EQ("/tmp/pipeline_test", last_cmd->redirect_file, "Should set correct redirect file");

        free_pipeline_info(&pipeline);
    }

    return 1;
}

TEST_SUITE_BEGIN("Redirection Parsing Tests")
    RUN_TEST(test_contains_shell_operators);
    RUN_TEST(test_parse_command_with_redirection);
    RUN_TEST(test_enhanced_parse_command);
    RUN_TEST(test_tokenize_command_line);
    RUN_TEST(test_trim_whitespace_inplace);
    RUN_TEST(test_redirection_integration);
    RUN_TEST(test_redirection_edge_cases);
    RUN_TEST(test_pipeline_redirection_integration);
TEST_SUITE_END()
