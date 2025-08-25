#include "test_framework.h"
#include "sudosh.h"
#include <sys/stat.h>
#include <pwd.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Helper to create a temporary rc file in test HOME */
static int write_user_rc(const char *filename, const char *content, mode_t mode) {
    char path[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) return 0;
    snprintf(path, sizeof(path), "%s/%s", home, filename);
    FILE *f = fopen(path, "w");
    if (!f) { return 0; }
    fputs(content, f);
    fclose(f);
    chmod(path, mode);
    return 1;
}

static void remove_user_rc(const char *filename) {
    char path[PATH_MAX];
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(path, sizeof(path), "%s/%s", home, filename);
    unlink(path);
}

int test_import_safe_aliases_from_rc() {
    printf("Running test_import_safe_aliases_from_rc... ");

    /* Create a safe alias in .zshrc */
    TEST_ASSERT_EQ(1, write_user_rc(".zshrc", "alias ll='ls -la'\n", 0644), "should write .zshrc");

    /* Initialize alias system (will load files) */
    init_alias_system();

    /* Explicitly try to load aliases from rc files */
    int loaded = load_aliases_from_shell_rc_files();
    TEST_ASSERT_EQ(1, loaded, "should load at least one alias from rc file");

    char *val = get_alias_value("ll");
    TEST_ASSERT_NOT_NULL(val, "alias ll should be present");
    if (val) {
        TEST_ASSERT(val && (strcmp(val, "ls -la") == 0 || strcmp(val, "ls -lA") == 0 || strcmp(val, "ls -lah") == 0 || strcmp(val, "ls -l") == 0), "alias value should match");
    }

    /* Cleanup */
    remove_user_rc(".zshrc");

    printf("PASS\n");
    return 1;
}

int test_reject_unsafe_aliases_from_rc() {
    printf("Running test_reject_unsafe_aliases_from_rc... ");

    /* Dangerous alias should be rejected */
    TEST_ASSERT_EQ(1, write_user_rc(".bashrc", "alias bad='rm -rf /'\n", 0644), "should write .bashrc");

    /* Re-load after writing */
    (void)load_aliases_from_shell_rc_files();
    /* Even if other aliases loaded, this one should not be present */
    char *val = get_alias_value("bad");
    TEST_ASSERT(val == NULL, "dangerous alias should not be imported");

    /* Now create an alias with command substitution which should be rejected */
    TEST_ASSERT_EQ(1, write_user_rc(".bashrc", "alias evil='echo $(id)'\n", 0644), "should overwrite .bashrc");
    (void)load_aliases_from_shell_rc_files();
    val = get_alias_value("evil");
    TEST_ASSERT(val == NULL, "alias with command substitution should not be imported");

    /* Insecure permissions should skip file */
    TEST_ASSERT_EQ(1, write_user_rc(".bashrc", "alias skip='echo ok'\n", 0666), "should write insecure perms");
    (void)load_aliases_from_shell_rc_files();
    val = get_alias_value("skip");
    TEST_ASSERT(val == NULL, "alias from insecure file should not be imported");

    remove_user_rc(".bashrc");
    printf("PASS\n");
    return 1;
}



int test_various_safe_alias_values() {
    printf("Running test_various_safe_alias_values... ");
    /* Multiple safe variants */
    TEST_ASSERT_EQ(1, write_user_rc(".zshrc", "alias a='echo hi'\nalias b=\"git status\"\n", 0644), "write .zshrc");
    (void)load_aliases_from_shell_rc_files();
    char *v1 = get_alias_value("a");
    char *v2 = get_alias_value("b");
    TEST_ASSERT(v1 && strcmp(v1, "echo hi") == 0, "alias a should import");
    TEST_ASSERT(v2 && strcmp(v2, "git status") == 0, "alias b should import");
    remove_user_rc(".zshrc");
    printf("PASS\n");
    return 1;
}

int test_quoted_edge_cases() {
    printf("Running test_quoted_edge_cases... ");
    /* Mismatched quotes should be ignored */
    TEST_ASSERT_EQ(1, write_user_rc(".bashrc", "alias bad='oops\n", 0644), "write .bashrc");
    (void)load_aliases_from_shell_rc_files();
    TEST_ASSERT(get_alias_value("bad") == NULL, "mismatched quote alias should not import");
    remove_user_rc(".bashrc");
    printf("PASS\n");
    return 1;
}

int main() {
    int passes = 0;
    int count = 4;
    passes += test_import_safe_aliases_from_rc();
    passes += test_reject_unsafe_aliases_from_rc();
    passes += test_various_safe_alias_values();
    passes += test_quoted_edge_cases();
    printf("\n=== RC Alias Import Tests ===\n");
    printf("Total tests: %d\n", count);
    printf("Passed: %d\n", passes);
    printf("Failed: %d\n", count - passes);
    return (passes == count) ? 0 : 1;
}

