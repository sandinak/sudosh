#include "../test_framework.h"
#include "../../src/editor_detection.h"
#include <stdlib.h>
#include <string.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static void _unset_editor_envs(void) {
    const char *vars[] = {
        /* VSCode */
        "VSCODE_PID", "VSCODE_IPC_HOOK", "VSCODE_IPC_HOOK_CLI",
        "VSCODE_INJECTION", "VSCODE_CWD", "VSCODE_NLS_CONFIG",
        /* JetBrains */
        "IDEA_INITIAL_DIRECTORY", "PYCHARM_HOSTED", "WEBSTORM_VM_OPTIONS",
        "INTELLIJ_ENVIRONMENT_READER",
        /* Other editors */
        "ATOM_HOME", "SUBLIME_TEXT_PATH", "CURSOR_USER_DATA_DIR",
        /* Terminal indicators in GUI */
        "GNOME_TERMINAL_SCREEN", "KONSOLE_DBUS_SESSION", "ITERM_SESSION_ID",
        "KITTY_WINDOW_ID", "ALACRITTY_SOCKET",
        /* Remote/SSH editors */
        "REMOTE_CONTAINERS", "CODESPACES", "GITPOD_WORKSPACE_ID",
        NULL
    };
    for (int i = 0; vars[i]; i++) unsetenv(vars[i]);
}

static int test_editor_env_vars_detection() {
    _unset_editor_envs();

    /* Baseline may be true in CI; only assert positive case */
    setenv("VSCODE_PID", "12345", 1);
    TEST_ASSERT(has_editor_environment_variables() == 1, "detects VSCODE env");
    unsetenv("VSCODE_PID");
    return 1;
}

static int test_editor_terminal_type_detection() {
    setenv("TERM", "xterm-256color", 1);
    TEST_ASSERT(has_editor_terminal_type() == 1, "detect editor terminal type");

    setenv("TERM", "dumb", 1);
    TEST_ASSERT(has_editor_terminal_type() == 0, "non-editor terminal type");
    return 1;
}

static int test_remote_editor_session_detection() {
    unsetenv("SSH_CLIENT");
    unsetenv("SSH_CONNECTION");
    unsetenv("CODESPACES");

    TEST_ASSERT(is_remote_editor_session() == 0, "no remote session by default");

    setenv("SSH_CLIENT", "1.2.3.4 22 34567", 1);
    setenv("VSCODE_PID", "999", 1);
    TEST_ASSERT(is_remote_editor_session() == 1, "ssh + editor env triggers remote session");

    unsetenv("SSH_CLIENT");
    unsetenv("VSCODE_PID");
    return 1;
}

TEST_SUITE_BEGIN("Editor Detection Unit Tests")
    RUN_TEST(test_editor_env_vars_detection);
    RUN_TEST(test_editor_terminal_type_detection);
    RUN_TEST(test_remote_editor_session_detection);
TEST_SUITE_END()

