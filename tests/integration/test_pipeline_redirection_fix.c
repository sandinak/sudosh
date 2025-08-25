#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <sys/stat.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test the exact issue that was reported */
int test_exact_reported_issue() {
    const char *test_command = "cat /etc/passwd | grep root > /tmp/test_redirection_fix.txt";
    struct pipeline_info pipeline;
    
    /* Clean up any existing test file */
    unlink("/tmp/test_redirection_fix.txt");
    
    /* Test that the command is detected as a pipeline */
    TEST_ASSERT_EQ(1, is_pipeline_command(test_command), 
                   "Command should be detected as pipeline");
    
    /* Parse the pipeline */
    int result = parse_pipeline(test_command, &pipeline);
    TEST_ASSERT_EQ(0, result, "Pipeline should parse successfully");
    
    if (result == 0) {
        TEST_ASSERT_EQ(2, pipeline.num_commands, "Should have 2 commands in pipeline");
        
        /* Check first command (cat /etc/passwd) */
        struct command_info *cmd1 = &pipeline.commands[0].cmd;
        TEST_ASSERT_STR_EQ("cat", cmd1->argv[0], "First command should be cat");
        TEST_ASSERT_EQ(REDIRECT_NONE, cmd1->redirect_type, "First command should have no redirection");
        
        /* Check second command (grep root > /tmp/test_redirection_fix.txt) */
        struct command_info *cmd2 = &pipeline.commands[1].cmd;
        TEST_ASSERT_STR_EQ("grep", cmd2->argv[0], "Second command should be grep");
        TEST_ASSERT_STR_EQ("root", cmd2->argv[1], "Second command should have 'root' argument");
        TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd2->redirect_type, "Second command should have output redirection");
        TEST_ASSERT_STR_EQ("/tmp/test_redirection_fix.txt", cmd2->redirect_file, "Should redirect to correct file");
        
        free_pipeline_info(&pipeline);
    }
    
    /* Clean up */
    unlink("/tmp/test_redirection_fix.txt");
    
    return 1;
}

/* Test redirection type parsing */
int test_redirection_type_parsing() {
    struct command_info cmd;
    
    /* Test output redirection */
    int result = parse_command_with_redirection("echo test > /tmp/output.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse output redirection");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd.redirect_type, "Should set REDIRECT_OUTPUT type");
    TEST_ASSERT_STR_EQ("/tmp/output.txt", cmd.redirect_file, "Should set correct file");
    TEST_ASSERT_EQ(0, cmd.redirect_append, "Should not set append flag");
    free_command_info(&cmd);
    
    /* Test append redirection */
    result = parse_command_with_redirection("echo test >> /tmp/append.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse append redirection");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT_APPEND, cmd.redirect_type, "Should set REDIRECT_OUTPUT_APPEND type");
    TEST_ASSERT_STR_EQ("/tmp/append.txt", cmd.redirect_file, "Should set correct file");
    TEST_ASSERT_EQ(1, cmd.redirect_append, "Should set append flag");
    free_command_info(&cmd);
    
    /* Test input redirection */
    result = parse_command_with_redirection("sort < /tmp/input.txt", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse input redirection");
    TEST_ASSERT_EQ(REDIRECT_INPUT, cmd.redirect_type, "Should set REDIRECT_INPUT type");
    TEST_ASSERT_STR_EQ("/tmp/input.txt", cmd.redirect_file, "Should set correct file");
    free_command_info(&cmd);
    
    return 1;
}

/* Test pipeline with various redirection types */
int test_pipeline_redirection_types() {
    struct pipeline_info pipeline;
    
    /* Test pipeline with output redirection */
    int result = parse_pipeline("echo hello | grep hello > /tmp/test_output.txt", &pipeline);
    TEST_ASSERT_EQ(0, result, "Should parse pipeline with output redirection");
    if (result == 0) {
        struct command_info *last_cmd = &pipeline.commands[1].cmd;
        TEST_ASSERT_EQ(REDIRECT_OUTPUT, last_cmd->redirect_type, "Last command should have output redirection");
        TEST_ASSERT_STR_EQ("/tmp/test_output.txt", last_cmd->redirect_file, "Should set correct output file");
        free_pipeline_info(&pipeline);
    }
    
    /* Test pipeline with append redirection */
    result = parse_pipeline("echo world | grep world >> /tmp/test_append.txt", &pipeline);
    TEST_ASSERT_EQ(0, result, "Should parse pipeline with append redirection");
    if (result == 0) {
        struct command_info *last_cmd = &pipeline.commands[1].cmd;
        TEST_ASSERT_EQ(REDIRECT_OUTPUT_APPEND, last_cmd->redirect_type, "Last command should have append redirection");
        TEST_ASSERT_STR_EQ("/tmp/test_append.txt", last_cmd->redirect_file, "Should set correct append file");
        free_pipeline_info(&pipeline);
    }
    
    /* Test pipeline with input redirection on first command */
    result = parse_pipeline("sort < /tmp/input.txt | head -5", &pipeline);
    TEST_ASSERT_EQ(0, result, "Should parse pipeline with input redirection");
    if (result == 0) {
        struct command_info *first_cmd = &pipeline.commands[0].cmd;
        TEST_ASSERT_EQ(REDIRECT_INPUT, first_cmd->redirect_type, "First command should have input redirection");
        TEST_ASSERT_STR_EQ("/tmp/input.txt", first_cmd->redirect_file, "Should set correct input file");
        free_pipeline_info(&pipeline);
    }
    
    return 1;
}

/* Test complex pipeline scenarios */
int test_complex_pipeline_redirection() {
    struct pipeline_info pipeline;
    
    /* Test three-command pipeline with redirection */
    const char *complex_cmd = "cat /etc/passwd | grep -v nologin | sort > /tmp/sorted_users.txt";
    int result = parse_pipeline(complex_cmd, &pipeline);
    TEST_ASSERT_EQ(0, result, "Should parse complex pipeline");
    
    if (result == 0) {
        TEST_ASSERT_EQ(3, pipeline.num_commands, "Should have 3 commands");
        
        /* Check first command */
        struct command_info *cmd1 = &pipeline.commands[0].cmd;
        TEST_ASSERT_STR_EQ("cat", cmd1->argv[0], "First command should be cat");
        TEST_ASSERT_EQ(REDIRECT_NONE, cmd1->redirect_type, "First command should have no redirection");
        
        /* Check second command */
        struct command_info *cmd2 = &pipeline.commands[1].cmd;
        TEST_ASSERT_STR_EQ("grep", cmd2->argv[0], "Second command should be grep");
        TEST_ASSERT_EQ(REDIRECT_NONE, cmd2->redirect_type, "Second command should have no redirection");
        
        /* Check third command */
        struct command_info *cmd3 = &pipeline.commands[2].cmd;
        TEST_ASSERT_STR_EQ("sort", cmd3->argv[0], "Third command should be sort");
        TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd3->redirect_type, "Third command should have output redirection");
        TEST_ASSERT_STR_EQ("/tmp/sorted_users.txt", cmd3->redirect_file, "Should redirect to correct file");
        
        free_pipeline_info(&pipeline);
    }
    
    return 1;
}

/* Test that redirection parsing doesn't break regular commands */
int test_regular_command_compatibility() {
    struct command_info cmd;
    
    /* Test regular command without redirection */
    int result = parse_command("ls -la", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse regular command");
    TEST_ASSERT_STR_EQ("ls", cmd.argv[0], "Should parse command name");
    TEST_ASSERT_STR_EQ("-la", cmd.argv[1], "Should parse argument");
    TEST_ASSERT_EQ(REDIRECT_NONE, cmd.redirect_type, "Should have no redirection");
    TEST_ASSERT_NULL(cmd.redirect_file, "Should have no redirect file");
    free_command_info(&cmd);
    
    /* Test command with arguments that contain > or < but not as operators */
    result = parse_command("echo 'value > 5'", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse command with quoted operators");
    TEST_ASSERT_STR_EQ("echo", cmd.argv[0], "Should parse command name");
    TEST_ASSERT_EQ(REDIRECT_NONE, cmd.redirect_type, "Should have no redirection for quoted operators");
    free_command_info(&cmd);
    
    return 1;
}

/* Test error handling and edge cases */
int test_redirection_error_handling() {
    struct command_info cmd;
    
    /* Test unsafe redirection (should fail) */
    int result = parse_command_with_redirection("echo malicious > /etc/passwd", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject unsafe redirection to /etc/passwd");
    
    /* Test redirection without target */
    result = parse_command_with_redirection("echo test >", &cmd);
    TEST_ASSERT_NE(0, result, "Should reject redirection without target");
    
    /* Test NULL inputs */
    result = parse_command_with_redirection(NULL, &cmd);
    TEST_ASSERT_NE(0, result, "Should handle NULL input");
    
    result = parse_command_with_redirection("echo test > /tmp/file", NULL);
    TEST_ASSERT_NE(0, result, "Should handle NULL cmd");
    
    return 1;
}

/* Regression test for the specific bug that was fixed */
int test_redirect_type_bug_regression() {
    struct command_info cmd;
    
    /* This is the exact scenario that was failing */
    int result = parse_command_with_redirection("grep root > /tmp/foo", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse grep with redirection");
    
    /* The bug was that redirect_type was 0 (REDIRECT_NONE) instead of 2 (REDIRECT_OUTPUT) */
    TEST_ASSERT_EQ(REDIRECT_OUTPUT, cmd.redirect_type, "CRITICAL: redirect_type must be REDIRECT_OUTPUT, not REDIRECT_NONE");
    TEST_ASSERT_STR_EQ("/tmp/foo", cmd.redirect_file, "Should set correct redirect file");
    TEST_ASSERT_STR_EQ("grep", cmd.argv[0], "Should parse command correctly");
    TEST_ASSERT_STR_EQ("root", cmd.argv[1], "Should parse argument correctly");
    TEST_ASSERT_EQ(2, cmd.argc, "Should have correct argument count");
    
    free_command_info(&cmd);
    
    /* Test append redirection as well */
    result = parse_command_with_redirection("echo test >> /tmp/append", &cmd);
    TEST_ASSERT_EQ(0, result, "Should parse append redirection");
    TEST_ASSERT_EQ(REDIRECT_OUTPUT_APPEND, cmd.redirect_type, "Should set REDIRECT_OUTPUT_APPEND type");
    TEST_ASSERT_EQ(1, cmd.redirect_append, "Should set append flag");
    
    free_command_info(&cmd);
    
    return 1;
}

TEST_SUITE_BEGIN("Pipeline Redirection Fix Tests")
    RUN_TEST(test_exact_reported_issue);
    RUN_TEST(test_redirection_type_parsing);
    RUN_TEST(test_pipeline_redirection_types);
    RUN_TEST(test_complex_pipeline_redirection);
    RUN_TEST(test_regular_command_compatibility);
    RUN_TEST(test_redirection_error_handling);
    RUN_TEST(test_redirect_type_bug_regression);
TEST_SUITE_END()
