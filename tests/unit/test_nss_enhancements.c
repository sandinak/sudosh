#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Test get_user_info_files with valid username */
int test_get_user_info_files_valid() {
    struct user_info *user = get_user_info_files("root");
    
    TEST_ASSERT_NOT_NULL(user, "get_user_info_files should return user info for root");
    TEST_ASSERT_STR_EQ("root", user->username, "Username should be root");
    TEST_ASSERT_EQ(0, user->uid, "Root UID should be 0");
    TEST_ASSERT_EQ(0, user->gid, "Root GID should be 0");
    TEST_ASSERT_NOT_NULL(user->home_dir, "Home directory should be set");
    TEST_ASSERT_NOT_NULL(user->shell, "Shell should be set");
    
    free_user_info(user);
    return 1;
}

/* Test get_user_info_files with invalid username */
int test_get_user_info_files_invalid() {
    struct user_info *user = get_user_info_files("nonexistent_user_12345");
    
    TEST_ASSERT_NULL(user, "get_user_info_files should return NULL for nonexistent user");
    
    return 1;
}

/* Test get_user_info_files with NULL input */
int test_get_user_info_files_null() {
    struct user_info *user = get_user_info_files(NULL);
    
    TEST_ASSERT_NULL(user, "get_user_info_files should return NULL for NULL input");
    
    return 1;
}

/* Test check_admin_groups_files with root user */
int test_check_admin_groups_files_root() {
    int is_admin = check_admin_groups_files("root");
    
    /* Root is typically in wheel or admin group */
    printf("  (root admin status: %s) ", is_admin ? "admin" : "not admin");
    
    /* We can't assert a specific result since it depends on system configuration */
    TEST_ASSERT(is_admin == 0 || is_admin == 1, 
                "check_admin_groups_files should return 0 or 1");
    
    return 1;
}

/* Test check_admin_groups_files with invalid user */
int test_check_admin_groups_files_invalid() {
    int is_admin = check_admin_groups_files("nonexistent_user_12345");
    
    TEST_ASSERT_EQ(0, is_admin, "Nonexistent user should not be admin");
    
    return 1;
}

/* Test check_admin_groups_files with NULL input */
int test_check_admin_groups_files_null() {
    int is_admin = check_admin_groups_files(NULL);
    
    TEST_ASSERT_EQ(0, is_admin, "NULL input should return 0");
    
    return 1;
}

/* Test check_sudo_privileges_nss functionality */
int test_check_sudo_privileges_nss() {
    /* Test with current user */
    char *username = getenv("USER");
    if (!username) username = "root";
    
    int has_privileges = check_sudo_privileges_nss(username);
    
    printf("  (user %s sudo privileges: %s) ", username, 
           has_privileges ? "has" : "does not have");
    
    /* We can't assert a specific result since it depends on system configuration */
    TEST_ASSERT(has_privileges == 0 || has_privileges == 1, 
                "check_sudo_privileges_nss should return 0 or 1");
    
    return 1;
}

/* Test check_sudo_privileges_nss with NULL input */
int test_check_sudo_privileges_nss_null() {
    int has_privileges = check_sudo_privileges_nss(NULL);
    
    TEST_ASSERT_EQ(0, has_privileges, "NULL input should return 0");
    
    return 1;
}

/* Test check_command_permission_nss functionality */
int test_check_command_permission_nss() {
    char *username = getenv("USER");
    if (!username) username = "root";
    
    /* Test with a safe command */
    int allowed = check_command_permission_nss(username, "ls");
    
    printf("  (user %s ls permission: %s) ", username, 
           allowed ? "allowed" : "denied");
    
    /* We can't assert a specific result since it depends on system configuration */
    TEST_ASSERT(allowed == 0 || allowed == 1, 
                "check_command_permission_nss should return 0 or 1");
    
    return 1;
}

/* Test check_command_permission_nss with NULL inputs */
int test_check_command_permission_nss_null() {
    int allowed1 = check_command_permission_nss(NULL, "ls");
    int allowed2 = check_command_permission_nss("root", NULL);
    int allowed3 = check_command_permission_nss(NULL, NULL);
    
    TEST_ASSERT_EQ(0, allowed1, "NULL username should return 0");
    TEST_ASSERT_EQ(0, allowed2, "NULL command should return 0");
    TEST_ASSERT_EQ(0, allowed3, "NULL inputs should return 0");
    
    return 1;
}

/* Test error handling for missing /etc/passwd */
int test_missing_passwd_file() {
    /* This test simulates missing /etc/passwd by testing the fallback mechanism */
    /* We can't actually remove /etc/passwd, so we test with a nonexistent user */
    
    struct user_info *user = get_user_info_files("user_that_definitely_does_not_exist_12345");
    
    TEST_ASSERT_NULL(user, "Should return NULL for nonexistent user");
    
    return 1;
}

/* Test NSS configuration reading */
int test_nss_config_reading() {
    struct nss_config *config = read_nss_config();
    
    /* NSS config should be readable or fallback to defaults */
    TEST_ASSERT_NOT_NULL(config, "NSS config should be readable or have defaults");
    
    if (config) {
        /* Should have at least passwd sources */
        TEST_ASSERT_NOT_NULL(config->passwd_sources, "Should have passwd sources");
        
        free_nss_config(config);
    }
    
    return 1;
}

/* Test get_user_info_nss with valid config */
int test_get_user_info_nss() {
    struct nss_config *config = read_nss_config();
    
    if (config) {
        struct user_info *user = get_user_info_nss("root", config);
        
        if (user) {
            TEST_ASSERT_STR_EQ("root", user->username, "Username should be root");
            TEST_ASSERT_EQ(0, user->uid, "Root UID should be 0");
            free_user_info(user);
        }
        
        free_nss_config(config);
    }
    
    return 1;
}

/* Test performance - NSS functions should not require sudo */
int test_nss_no_sudo_dependency() {
    /* Test that NSS functions work without sudo privileges */
    /* This is more of a conceptual test - the functions should work */
    
    struct user_info *user = get_user_info_files("root");
    TEST_ASSERT_NOT_NULL(user, "Should work without sudo dependency");
    
    if (user) {
        free_user_info(user);
    }
    
    int is_admin = check_admin_groups_files("root");
    TEST_ASSERT(is_admin == 0 || is_admin == 1, "Should work without sudo dependency");
    
    return 1;
}

TEST_SUITE_BEGIN("NSS Enhancement Tests")
    RUN_TEST(test_get_user_info_files_valid);
    RUN_TEST(test_get_user_info_files_invalid);
    RUN_TEST(test_get_user_info_files_null);
    RUN_TEST(test_check_admin_groups_files_root);
    RUN_TEST(test_check_admin_groups_files_invalid);
    RUN_TEST(test_check_admin_groups_files_null);
    RUN_TEST(test_check_sudo_privileges_nss);
    RUN_TEST(test_check_sudo_privileges_nss_null);
    RUN_TEST(test_check_command_permission_nss);
    RUN_TEST(test_check_command_permission_nss_null);
    RUN_TEST(test_missing_passwd_file);
    RUN_TEST(test_nss_config_reading);
    RUN_TEST(test_get_user_info_nss);
    RUN_TEST(test_nss_no_sudo_dependency);
TEST_SUITE_END()
