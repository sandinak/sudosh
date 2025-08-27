#include "../tests/test_framework.h"
#include "../src/sudosh.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static int test_init_file_locking_in_test_mode() {
    setenv("SUDOSH_TEST_MODE", "1", 1);
    int rc = init_file_locking();
    TEST_ASSERT(rc == 0, "init_file_locking returns 0 in test mode");
    TEST_ASSERT(is_file_locking_available() == 1, "file locking available after init");
    cleanup_file_locking();
    unsetenv("SUDOSH_TEST_MODE");
    return 1;
}

static int test_acquire_and_release_lock() {
    setenv("SUDOSH_TEST_MODE", "1", 1);
    (void)init_file_locking();

    const char *tmpfile = "/tmp/sudosh_lock_test_file.txt";
    /* Ensure file exists so realpath can resolve */
    int fd = open(tmpfile, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    char *user = get_current_username();
    pid_t pid = getpid();

    int rc1 = acquire_file_lock(tmpfile, user ? user : "testuser", pid);
    TEST_ASSERT(rc1 == 0, "first acquire should succeed (0 on success)");

    /* Second acquire should fail as already locked */
    int rc2 = acquire_file_lock(tmpfile, user ? user : "other", pid + 1);
    TEST_ASSERT(rc2 != 0, "second acquire should report locked (non-zero)");

    int rc3 = release_file_lock(tmpfile, user ? user : "testuser", pid);
    TEST_ASSERT(rc3 == 0, "release should succeed (0 on success)");

    cleanup_file_locking();
    unsetenv("SUDOSH_TEST_MODE");
    unlink(tmpfile);
    return 1;
}

static int test_check_file_lock_info() {
    setenv("SUDOSH_TEST_MODE", "1", 1);
    (void)init_file_locking();

    const char *tmpfile = "/tmp/sudosh_lock_test_info.txt";
    int fd = open(tmpfile, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) close(fd);

    char *user = get_current_username();
    pid_t pid = getpid();
    (void)acquire_file_lock(tmpfile, user ? user : "testuser", pid);

    struct file_lock_info *info = check_file_lock(tmpfile);
    TEST_ASSERT(info != NULL, "check_file_lock returns info");
    if (info) {
        TEST_ASSERT(info->file_path != NULL, "info has file_path");
        TEST_ASSERT(info->username != NULL, "info has username");
        TEST_ASSERT(info->pid > 0, "info has pid");
        free_file_lock_info(info);
    }

    (void)release_file_lock(tmpfile, user ? user : "testuser", pid);

    cleanup_file_locking();
    unsetenv("SUDOSH_TEST_MODE");
    unlink(tmpfile);
    return 1;
}

TEST_SUITE_BEGIN("File Locking Unit Tests")
    RUN_TEST(test_init_file_locking_in_test_mode);
    RUN_TEST(test_acquire_and_release_lock);
    RUN_TEST(test_check_file_lock_info);
TEST_SUITE_END()

