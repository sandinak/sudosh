#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <sys/stat.h>
#include <unistd.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test is_text_processing_command function */
int test_is_text_processing_command() {
    /* Test text processing commands */
    TEST_ASSERT_EQ(1, is_text_processing_command("grep"), "grep should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("sed"), "sed should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("awk"), "awk should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("gawk"), "gawk should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("egrep"), "egrep should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("fgrep"), "fgrep should be text processing command");
    
    /* Test with full paths */
    TEST_ASSERT_EQ(1, is_text_processing_command("/bin/grep"), "Full path grep should be text processing command");
    TEST_ASSERT_EQ(1, is_text_processing_command("/usr/bin/sed"), "Full path sed should be text processing command");
    
    /* Test non-text processing commands */
    TEST_ASSERT_EQ(0, is_text_processing_command("ls"), "ls should not be text processing command");
    TEST_ASSERT_EQ(0, is_text_processing_command("rm"), "rm should not be text processing command");
    TEST_ASSERT_EQ(0, is_text_processing_command("cat"), "cat should not be text processing command");
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, is_text_processing_command(NULL), "NULL should not be text processing command");
    
    return 1;
}

/* Test validate_text_processing_command with safe commands */
int test_validate_text_processing_safe() {
    /* Test safe grep commands */
    TEST_ASSERT_EQ(1, validate_text_processing_command("grep 'pattern' file.txt"), 
                   "Safe grep command should be valid");
    TEST_ASSERT_EQ(1, validate_text_processing_command("grep -i 'pattern' file.txt"), 
                   "Safe grep with flags should be valid");
    
    /* Test safe sed commands */
    TEST_ASSERT_EQ(1, validate_text_processing_command("sed 's/old/new/g' file.txt"), 
                   "Safe sed substitution should be valid");
    TEST_ASSERT_EQ(1, validate_text_processing_command("sed -n '1,10p' file.txt"), 
                   "Safe sed print should be valid");
    
    /* Test safe awk commands */
    TEST_ASSERT_EQ(1, validate_text_processing_command("awk '{print $1}' file.txt"), 
                   "Safe awk print should be valid");
    TEST_ASSERT_EQ(1, validate_text_processing_command("awk 'NR==1 {print}' file.txt"), 
                   "Safe awk condition should be valid");
    
    return 1;
}

/* Test validate_text_processing_command with dangerous sed commands */
int test_validate_text_processing_dangerous_sed() {
    /* Test dangerous sed commands that can execute shell commands */
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 'e /bin/sh' file.txt"), 
                   "sed with 'e' command should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 'w /etc/passwd' file.txt"), 
                   "sed with 'w' command should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 'r /etc/shadow' file.txt"), 
                   "sed with 'r' command should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 'W /tmp/output' file.txt"), 
                   "sed with 'W' command should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 'R /etc/hosts' file.txt"), 
                   "sed with 'R' command should be blocked");
    
    return 1;
}

/* Test validate_text_processing_command with dangerous awk commands */
int test_validate_text_processing_dangerous_awk() {
    /* Test dangerous awk commands with system() calls */
    TEST_ASSERT_EQ(0, validate_text_processing_command("awk 'BEGIN{system(\"ls\")}' file.txt"), 
                   "awk with system() call should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("awk '{system(\"rm file\")}' file.txt"), 
                   "awk with system() in action should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("awk 'system \"whoami\"' file.txt"), 
                   "awk with system command should be blocked");
    
    return 1;
}

/* Test validate_text_processing_command with dangerous grep commands */
int test_validate_text_processing_dangerous_grep() {
    /* Test dangerous grep commands */
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep -e 'pattern' --exec=rm file.txt"), 
                   "grep with exec flag should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep --include=/etc/passwd pattern"), 
                   "grep with dangerous include should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep --include=/var/log/* pattern"), 
                   "grep with system directory include should be blocked");
    
    return 1;
}

/* Test validate_text_processing_command with command injection */
int test_validate_text_processing_command_injection() {
    /* Test command substitution */
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep `whoami` file.txt"), 
                   "Command substitution with backticks should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("sed 's/old/$(whoami)/g' file.txt"), 
                   "Command substitution with $() should be blocked");
    
    /* Test shell metacharacters */
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep pattern; rm file"), 
                   "Command chaining with semicolon should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep pattern && rm file"), 
                   "Command chaining with && should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep pattern || rm file"), 
                   "Command chaining with || should be blocked");
    TEST_ASSERT_EQ(0, validate_text_processing_command("grep pattern & rm file"), 
                   "Background execution should be blocked");
    
    return 1;
}

/* Test is_safe_redirection_target function */
int test_is_safe_redirection_target() {
    /* Test safe targets */
    TEST_ASSERT_EQ(1, is_safe_redirection_target("/tmp/test.txt"), 
                   "/tmp/ should be safe redirection target");
    TEST_ASSERT_EQ(1, is_safe_redirection_target("/var/tmp/test.txt"), 
                   "/var/tmp/ should be safe redirection target");
    TEST_ASSERT_EQ(1, is_safe_redirection_target("test.txt"), 
                   "Relative path should be safe redirection target");
    TEST_ASSERT_EQ(1, is_safe_redirection_target("./test.txt"), 
                   "Current directory should be safe redirection target");
    
    /* Test home directory expansion */
    TEST_ASSERT_EQ(1, is_safe_redirection_target("~/test.txt"), 
                   "Home directory should be safe redirection target");
    
    /* Test unsafe targets */
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/etc/passwd"), 
                   "/etc/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/usr/bin/test"), 
                   "/usr/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/var/log/test.log"), 
                   "/var/log/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/root/test.txt"), 
                   "/root/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/sys/test"), 
                   "/sys/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/proc/test"), 
                   "/proc/ should not be safe redirection target");
    TEST_ASSERT_EQ(0, is_safe_redirection_target("/dev/null"), 
                   "/dev/ should not be safe redirection target");
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, is_safe_redirection_target(NULL), 
                   "NULL should not be safe redirection target");
    
    return 1;
}

/* Test validate_safe_redirection function */
int test_validate_safe_redirection() {
    /* Test safe redirections */
    TEST_ASSERT_EQ(1, validate_safe_redirection("echo test > /tmp/safe.txt"), 
                   "Redirection to /tmp/ should be safe");
    TEST_ASSERT_EQ(1, validate_safe_redirection("cat file >> /var/tmp/output.txt"), 
                   "Append to /var/tmp/ should be safe");
    TEST_ASSERT_EQ(1, validate_safe_redirection("grep pattern < input.txt"), 
                   "Input redirection should be safe");
    TEST_ASSERT_EQ(1, validate_safe_redirection("sort data > ~/output.txt"), 
                   "Redirection to home directory should be safe");
    
    /* Test unsafe redirections */
    TEST_ASSERT_EQ(0, validate_safe_redirection("echo malicious > /etc/passwd"), 
                   "Redirection to /etc/passwd should be unsafe");
    TEST_ASSERT_EQ(0, validate_safe_redirection("cat data >> /var/log/system.log"), 
                   "Append to system log should be unsafe");
    TEST_ASSERT_EQ(0, validate_safe_redirection("echo test > /usr/bin/malicious"), 
                   "Redirection to /usr/bin/ should be unsafe");
    
    /* Test commands without redirection */
    TEST_ASSERT_EQ(1, validate_safe_redirection("grep pattern file.txt"), 
                   "Command without redirection should be safe");
    
    /* Test malformed redirections */
    TEST_ASSERT_EQ(0, validate_safe_redirection("echo test >"), 
                   "Redirection without target should be unsafe");
    
    /* Test NULL input */
    TEST_ASSERT_EQ(0, validate_safe_redirection(NULL), 
                   "NULL input should be unsafe");
    
    return 1;
}

/* Test text processing commands in safe command list */
int test_text_processing_in_safe_commands() {
    /* Test that text processing commands are now considered safe */
    TEST_ASSERT_EQ(1, is_safe_command("grep pattern file.txt"), 
                   "grep should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("sed 's/old/new/' file.txt"), 
                   "sed should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("awk '{print $1}' file.txt"), 
                   "awk should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("cut -d: -f1 file.txt"), 
                   "cut should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("sort file.txt"), 
                   "sort should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("uniq file.txt"), 
                   "uniq should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("head -10 file.txt"), 
                   "head should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("tail -10 file.txt"), 
                   "tail should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("wc -l file.txt"), 
                   "wc should be safe command");
    TEST_ASSERT_EQ(1, is_safe_command("cat file.txt"), 
                   "cat should be safe command");
    
    return 1;
}

/* Test text processing commands with dangerous patterns are blocked */
int test_text_processing_dangerous_blocked() {
    /* These should be blocked by the enhanced validation */
    TEST_ASSERT_EQ(0, is_safe_command("sed 'e /bin/sh' file.txt"), 
                   "Dangerous sed command should be blocked");
    TEST_ASSERT_EQ(0, is_safe_command("awk 'BEGIN{system(\"rm -rf /\")}' file.txt"), 
                   "Dangerous awk command should be blocked");
    TEST_ASSERT_EQ(0, is_safe_command("grep --exec=rm pattern file.txt"), 
                   "Dangerous grep command should be blocked");
    
    return 1;
}

/* Test redirection with text processing commands */
int test_text_processing_with_redirection() {
    /* Test safe combinations */
    TEST_ASSERT_EQ(1, validate_safe_redirection("grep pattern file.txt > /tmp/results.txt"), 
                   "grep with safe redirection should be allowed");
    TEST_ASSERT_EQ(1, validate_safe_redirection("sed 's/old/new/g' input.txt > ~/output.txt"), 
                   "sed with safe redirection should be allowed");
    TEST_ASSERT_EQ(1, validate_safe_redirection("awk '{print $1}' data.txt >> /var/tmp/summary.txt"), 
                   "awk with safe redirection should be allowed");
    
    /* Test unsafe combinations */
    TEST_ASSERT_EQ(0, validate_safe_redirection("grep password /etc/shadow > /tmp/stolen.txt"), 
                   "grep with unsafe source should be blocked by other mechanisms");
    TEST_ASSERT_EQ(0, validate_safe_redirection("sed 's/old/new/g' file.txt > /etc/passwd"), 
                   "sed with unsafe redirection should be blocked");
    
    return 1;
}

/* Test edge cases and error handling */
int test_redirection_edge_cases() {
    /* Test multiple redirections */
    TEST_ASSERT_EQ(0, validate_safe_redirection("echo test > file1.txt > file2.txt"), 
                   "Multiple redirections should be handled safely");
    
    /* Test redirection with spaces */
    TEST_ASSERT_EQ(1, validate_safe_redirection("echo test >   /tmp/spaced.txt"), 
                   "Redirection with spaces should work");
    
    /* Test redirection with tabs */
    TEST_ASSERT_EQ(1, validate_safe_redirection("echo test >\t/tmp/tabbed.txt"), 
                   "Redirection with tabs should work");
    
    return 1;
}

TEST_SUITE_BEGIN("Text Processing and Redirection Tests")
    RUN_TEST(test_is_text_processing_command);
    RUN_TEST(test_validate_text_processing_safe);
    RUN_TEST(test_validate_text_processing_dangerous_sed);
    RUN_TEST(test_validate_text_processing_dangerous_awk);
    RUN_TEST(test_validate_text_processing_dangerous_grep);
    RUN_TEST(test_validate_text_processing_command_injection);
    RUN_TEST(test_is_safe_redirection_target);
    RUN_TEST(test_validate_safe_redirection);
    RUN_TEST(test_text_processing_in_safe_commands);
    RUN_TEST(test_text_processing_dangerous_blocked);
    RUN_TEST(test_text_processing_with_redirection);
    RUN_TEST(test_redirection_edge_cases);
TEST_SUITE_END()
