#include "editor_detection.h"
#include "sudosh.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/* Editor process names that indicate interactive editor environments */
static const char *editor_process_names[] = {
    /* VSCode variants */
    "code", "code-server", "vscode-server", "code-insiders",
    "code-oss", "vscodium",
    
    /* JetBrains IDEs */
    "idea", "intellij", "pycharm", "webstorm", "phpstorm",
    "rubymine", "clion", "datagrip", "goland", "rider",
    "android-studio",
    
    /* Other popular editors */
    "cursor", "atom", "sublime_text", "subl",
    "brackets", "notepad++", "gedit", "kate",
    "emacs", "vim", "nvim", "nano",
    
    /* Cloud/Web IDEs */
    "theia", "gitpod", "codespaces", "cloud9",
    "replit", "codesandbox",
    
    /* Terminal-based editors in GUI contexts */
    "gnome-terminal", "konsole", "xterm", "alacritty",
    "kitty", "iterm2", "hyper", "terminus",
    
    NULL
};

/* Environment variables that indicate editor environments */
static const char *editor_env_vars[] = {
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

/* Terminal types that suggest GUI/editor environments */
static const char *editor_terminal_types[] = {
    "xterm-256color", "screen-256color", "tmux-256color",
    "vt100", "vt220", "linux", "ansi",
    NULL
};

/**
 * Get the parent process ID for a given process
 */
static pid_t get_parent_pid(pid_t pid) {
#ifdef __linux__
    FILE *stat_file;
    char stat_path[64];
    pid_t ppid = 0;

    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    stat_file = fopen(stat_path, "r");
    if (stat_file) {
        int proc_pid;
        char comm[256];
        char state;

        if (fscanf(stat_file, "%d %s %c %d", &proc_pid, comm, &state, &ppid) == 4) {
            /* Successfully read parent PID */
        }
        fclose(stat_file);
    }
    return ppid;
#else
    /* Fallback for non-Linux systems */
    if (pid == getpid()) {
        return getppid();
    }
    return 0;
#endif
}

/**
 * Get the process name for a given PID
 */
static char *get_process_name(pid_t pid) {
#ifdef __linux__
    FILE *comm_file;
    char comm_path[64];
    char *process_name = NULL;

    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    comm_file = fopen(comm_path, "r");
    if (comm_file) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), comm_file)) {
            /* Remove newline */
            char *newline = strchr(buffer, '\n');
            if (newline) *newline = '\0';
            process_name = strdup(buffer);
        }
        fclose(comm_file);
    }
    return process_name;
#else
    /* Fallback - return NULL for non-Linux systems */
    return NULL;
#endif
}

/**
 * Check if current environment has editor-specific environment variables
 */
int has_editor_environment_variables(void) {
    for (int i = 0; editor_env_vars[i]; i++) {
        if (getenv(editor_env_vars[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * Check if current terminal type suggests an editor environment
 */
int has_editor_terminal_type(void) {
    const char *term = getenv("TERM");
    if (!term) return 0;
    
    for (int i = 0; editor_terminal_types[i]; i++) {
        if (strcmp(term, editor_terminal_types[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * Walk up the process tree looking for editor processes
 */
int has_editor_in_process_tree(void) {
    pid_t current_pid = getpid();
    int depth = 0;
    const int max_depth = 10;  /* Limit search depth */
    
    while (current_pid > 1 && depth < max_depth) {
        char *process_name = get_process_name(current_pid);
        if (process_name) {
            /* Check if this process is an editor */
            for (int i = 0; editor_process_names[i]; i++) {
                if (strcmp(process_name, editor_process_names[i]) == 0 ||
                    strstr(process_name, editor_process_names[i]) != NULL) {
                    free(process_name);
                    return 1;
                }
            }
            free(process_name);
        }
        
        /* Move to parent process */
        pid_t parent_pid = get_parent_pid(current_pid);
        if (parent_pid <= 1 || parent_pid == current_pid) {
            break;  /* No valid parent or reached init */
        }
        current_pid = parent_pid;
        depth++;
    }
    
    return 0;
}

/**
 * Check if we're running in a remote/SSH editor session
 */
int is_remote_editor_session(void) {
    /* Check for SSH connection with editor indicators */
    if (getenv("SSH_CLIENT") || getenv("SSH_CONNECTION")) {
        /* SSH session - check if it's an editor session */
        if (has_editor_environment_variables()) {
            return 1;
        }
        
        /* Check for remote editor specific variables */
        if (getenv("REMOTE_CONTAINERS") || getenv("CODESPACES") || 
            getenv("GITPOD_WORKSPACE_ID")) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * Main function to detect if we're running in an interactive editor environment
 */
int is_interactive_editor_environment(void) {
    /* Check environment variables first (fastest) */
    if (has_editor_environment_variables()) {
        return 1;
    }
    
    /* Check for remote editor sessions */
    if (is_remote_editor_session()) {
        return 1;
    }
    
    /* Check process tree for editor processes */
    if (has_editor_in_process_tree()) {
        return 1;
    }
    
    /* Check terminal type as a weaker indicator */
    if (has_editor_terminal_type()) {
        /* Terminal type alone is not enough, need additional indicators */
        /* Check if we have a GUI session */
        if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) {
            return 1;
        }
    }
    
    return 0;
}

/**
 * Get detailed information about the detected editor environment
 */
struct editor_detection_info *get_editor_detection_info(void) {
    struct editor_detection_info *info = malloc(sizeof(struct editor_detection_info));
    if (!info) return NULL;
    
    memset(info, 0, sizeof(struct editor_detection_info));
    
    /* Check each detection method */
    info->has_env_vars = has_editor_environment_variables();
    info->has_editor_process = has_editor_in_process_tree();
    info->is_remote_session = is_remote_editor_session();
    info->has_gui_session = (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY")) ? 1 : 0;
    
    /* Determine overall result */
    info->is_editor_environment = is_interactive_editor_environment();
    
    /* Set confidence level */
    if (info->has_env_vars) {
        info->confidence_level = 90;  /* High confidence */
    } else if (info->has_editor_process) {
        info->confidence_level = 80;  /* High confidence */
    } else if (info->is_remote_session) {
        info->confidence_level = 70;  /* Medium-high confidence */
    } else if (info->has_gui_session && has_editor_terminal_type()) {
        info->confidence_level = 60;  /* Medium confidence */
    } else {
        info->confidence_level = 0;   /* No editor detected */
    }
    
    /* Build detection details string */
    snprintf(info->detection_details, sizeof(info->detection_details),
             "Env vars: %s, Process: %s, Remote: %s, GUI: %s",
             info->has_env_vars ? "yes" : "no",
             info->has_editor_process ? "yes" : "no", 
             info->is_remote_session ? "yes" : "no",
             info->has_gui_session ? "yes" : "no");
    
    return info;
}
