/**
 * ansible_detection.c - Ansible Session Detection
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file implements detection mechanisms to identify when sudosh
 * is being called from an Ansible automation session versus a regular
 * interactive user session.
 */

#include "sudosh.h"

/* External variables */
extern char **environ;

/* List of environment variables that indicate Ansible execution */
static const char *ansible_env_vars[] = {
    "ANSIBLE_HOST_KEY_CHECKING",
    "ANSIBLE_PYTHON_INTERPRETER",
    "ANSIBLE_REMOTE_USER",
    "ANSIBLE_CONFIG",
    "ANSIBLE_INVENTORY",
    "ANSIBLE_PLAYBOOK_DIR",
    "ANSIBLE_ROLES_PATH",
    "ANSIBLE_VAULT_PASSWORD_FILE",
    "ANSIBLE_FORCE_COLOR",
    "ANSIBLE_STDOUT_CALLBACK",
    "ANSIBLE_CALLBACK_PLUGINS",
    "ANSIBLE_ACTION_PLUGINS",
    "ANSIBLE_LIBRARY",
    "ANSIBLE_MODULE_UTILS",
    "ANSIBLE_FILTER_PLUGINS",
    "ANSIBLE_LOOKUP_PLUGINS",
    "ANSIBLE_STRATEGY_PLUGINS",
    "ANSIBLE_CONNECTION_PLUGINS",
    "ANSIBLE_VARS_PLUGINS",
    "ANSIBLE_CACHE_PLUGIN",
    /* Become method specific variables */
    "ANSIBLE_BECOME_METHOD",
    "ANSIBLE_BECOME_USER",
    "ANSIBLE_MODULE_NAME",
    "ANSIBLE_TASK_UUID",
    "ANSIBLE_PLAYBOOK_UUID",
    "ANSIBLE_RUNNER_JOB_ID",
    "ANSIBLE_RUNNER_ARTIFACT_DIR",
    NULL
};

/* AI-specific detection has been moved to ai_detection.c */

/* Process names that indicate Ansible execution */
static const char *ansible_process_names[] = {
    "ansible-playbook",
    "ansible-runner",
    "ansible",
    "ansible-pull",
    "ansible-console",
    "ansible-vault",
    "ansible-galaxy",
    "python",  /* May be running ansible modules */
    "python3", /* May be running ansible modules */
    NULL
};

/* AI-specific process detection has been moved to ai_detection.c */

/**
 * Check if an environment variable name indicates Ansible
 */
int is_ansible_environment_variable(const char *var_name) {
    if (!var_name) {
        return 0;
    }

    /* Check exact matches first */
    for (int i = 0; ansible_env_vars[i]; i++) {
        if (strcmp(var_name, ansible_env_vars[i]) == 0) {
            return 1;
        }
    }

    /* Check for ANSIBLE_ prefix */
    if (strncmp(var_name, "ANSIBLE_", 8) == 0) {
        return 1;
    }

    return 0;
}

/* AI-specific environment variable checking has been moved to ai_detection.c */

/**
 * Check if sudosh is being used as an Ansible become method
 */
int check_ansible_become_method(struct ansible_detection_info *info) {
    if (!info) {
        return 0;
    }

    const char *become_method = getenv("ANSIBLE_BECOME_METHOD");
    const char *become_user = getenv("ANSIBLE_BECOME_USER");
    const char *module_name = getenv("ANSIBLE_MODULE_NAME");
    const char *task_uuid = getenv("ANSIBLE_TASK_UUID");

    int confidence = 0;

    /* Check if explicitly set as become method */
    if (become_method && (strcmp(become_method, "sudosh") == 0)) {
        confidence += 95;  /* Very high confidence */
        snprintf(info->detection_details, sizeof(info->detection_details),
                "Sudosh explicitly set as Ansible become method");
        info->method = ANSIBLE_DETECTED_ENV_VAR;
        strcpy(info->automation_type, "ansible");
    }

    /* Check for other become method indicators */
    if (become_user) {
        confidence += 20;
        if (strlen(info->detection_details) > 0) {
            strncat(info->detection_details, ", ", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
        }
        strncat(info->detection_details, "become_user set", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
    }

    if (module_name) {
        confidence += 15;
        if (strlen(info->detection_details) > 0) {
            strncat(info->detection_details, ", ", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
        }
        strncat(info->detection_details, "module context", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
    }

    if (task_uuid) {
        confidence += 10;
        if (strlen(info->detection_details) > 0) {
            strncat(info->detection_details, ", ", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
        }
        strncat(info->detection_details, "task UUID", sizeof(info->detection_details) - strlen(info->detection_details) - 1);
    }

    /* Cap confidence at 100 */
    if (confidence > 100) {
        confidence = 100;
    }

    return confidence;
}

/**
 * Check environment variables for Ansible indicators
 */
int check_ansible_environment_variables(struct ansible_detection_info *info) {
    if (!info) {
        return 0;
    }

    int ansible_vars_found = 0;
    info->env_var_count = 0;

    /* Check all environment variables */
    for (char **env = environ; *env && info->env_var_count < ANSIBLE_ENV_VAR_COUNT; env++) {
        char *var_copy = strdup(*env);
        if (!var_copy) continue;

        char *equals = strchr(var_copy, '=');
        if (equals) {
            *equals = '\0';  /* Null-terminate the variable name */

            if (is_ansible_environment_variable(var_copy)) {
                /* Store the variable name */
                strncpy(info->detected_env_vars[info->env_var_count],
                       var_copy, 63);
                info->detected_env_vars[info->env_var_count][63] = '\0';
                info->env_var_count++;
                ansible_vars_found++;
            }
        }

        free(var_copy);
    }

    /* Set automation type based on what we found */
    if (ansible_vars_found > 0) {
        strcpy(info->automation_type, "ansible");
        return ansible_vars_found;
    } else {
        strcpy(info->automation_type, "unknown");
        return 0;
    }
}

/**
 * Get the parent process ID for a given process (platform-agnostic)
 */
static pid_t get_process_parent_pid(pid_t pid) {
    pid_t ppid = 0;

#ifdef __linux__
    FILE *stat_file;
    char stat_path[64];

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

#elif defined(__FreeBSD__)
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc proc_info;
    size_t size = sizeof(proc_info);

    if (sysctl(mib, 4, &proc_info, &size, NULL, 0) == 0) {
        ppid = proc_info.ki_ppid;
    }

#elif defined(__OpenBSD__)
    int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), 1};
    struct kinfo_proc proc_info;
    size_t size = sizeof(proc_info);

    if (sysctl(mib, 6, &proc_info, &size, NULL, 0) == 0) {
        ppid = proc_info.p_ppid;
    }

#elif defined(__NetBSD__)
    int mib[4] = {CTL_KERN, KERN_PROC2, KERN_PROC_PID, pid};
    struct kinfo_proc2 proc_info;
    size_t size = sizeof(proc_info);

    if (sysctl(mib, 4, &proc_info, &size, NULL, 0) == 0) {
        ppid = proc_info.p_ppid;
    }

#elif defined(__APPLE__) && defined(__MACH__)
    struct proc_bsdinfo proc_info;
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
        ppid = proc_info.pbi_ppid;
    }

#else
    /* Fallback for other systems - only works for current process */
    if (pid == getpid()) {
        ppid = getppid();
    }
#endif

    return ppid;
}

/**
 * Get the parent process ID of the current process
 */
pid_t get_parent_process_id(void) {
    return get_process_parent_pid(getpid());
}

/**
 * Get the process name for a given PID
 */
char *get_parent_process_name(pid_t pid) {
    if (pid <= 0) {
        return NULL;
    }
    
#ifdef __linux__
    char proc_path[64];
    char *process_name = NULL;
    FILE *comm_file;
    
    /* Read /proc/PID/comm for process name */
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/comm", pid);
    comm_file = fopen(proc_path, "r");
    if (comm_file) {
        char comm_buffer[MAX_PROCESS_NAME_LENGTH];
        if (fgets(comm_buffer, sizeof(comm_buffer), comm_file)) {
            /* Remove trailing newline */
            char *newline = strchr(comm_buffer, '\n');
            if (newline) {
                *newline = '\0';
            }
            process_name = strdup(comm_buffer);
        }
        fclose(comm_file);
    }
    
    /* If comm failed, try reading cmdline */
    if (!process_name) {
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", pid);
        FILE *cmdline_file = fopen(proc_path, "r");
        if (cmdline_file) {
            char cmdline_buffer[MAX_PROCESS_NAME_LENGTH];
            if (fgets(cmdline_buffer, sizeof(cmdline_buffer), cmdline_file)) {
                /* Extract just the command name */
                char *space = strchr(cmdline_buffer, ' ');
                if (space) {
                    *space = '\0';
                }
                /* Get basename */
                char *basename = strrchr(cmdline_buffer, '/');
                if (basename) {
                    process_name = strdup(basename + 1);
                } else {
                    process_name = strdup(cmdline_buffer);
                }
            }
            fclose(cmdline_file);
        }
    }
    
    return process_name;
    
#elif defined(__APPLE__) && defined(__MACH__)
    /* macOS implementation using libproc */
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    char *process_name = NULL;
    
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
        char *basename = strrchr(pathbuf, '/');
        if (basename) {
            process_name = strdup(basename + 1);
        } else {
            process_name = strdup(pathbuf);
        }
    }
    
    return process_name;
    
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    /* BSD implementation using sysctl */
    char *process_name = NULL;
    size_t size;

#ifdef __FreeBSD__
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    struct kinfo_proc proc_info;
    size = sizeof(proc_info);

    if (sysctl(mib, 4, &proc_info, &size, NULL, 0) == 0) {
        process_name = strdup(proc_info.ki_comm);
    }
#elif defined(__OpenBSD__)
    int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), 1};
    struct kinfo_proc proc_info;
    size = sizeof(proc_info);

    if (sysctl(mib, 6, &proc_info, &size, NULL, 0) == 0) {
        process_name = strdup(proc_info.p_comm);
    }
#elif defined(__NetBSD__)
    int mib[4] = {CTL_KERN, KERN_PROC2, KERN_PROC_PID, pid};
    struct kinfo_proc2 proc_info;
    size = sizeof(proc_info);

    if (sysctl(mib, 4, &proc_info, &size, NULL, 0) == 0) {
        process_name = strdup(proc_info.p_comm);
    }
#endif

    return process_name;
    
#else
    /* Fallback for other systems */
    return NULL;
#endif
}

/**
 * Walk up the process tree looking for Ansible or Augment processes
 */
int walk_process_tree_for_ansible(pid_t start_pid, int max_depth) {
    pid_t current_pid = start_pid;
    int depth = 0;

    while (current_pid > 1 && depth < max_depth) {
        char *process_name = get_parent_process_name(current_pid);
        if (process_name) {
            /* Check if this process name indicates Ansible */
            for (int i = 0; ansible_process_names[i]; i++) {
                if (strcmp(process_name, ansible_process_names[i]) == 0) {
                    free(process_name);
                    return 1;  /* Found Ansible in process tree */
                }
            }

            /* AI-specific process detection has been moved to ai_detection.c */

            /* Special case for python/node processes - check if they could be automation */
            if ((strcmp(process_name, "python") == 0 ||
                 strcmp(process_name, "python3") == 0 ||
                 strcmp(process_name, "node") == 0)) {
                /* This is a heuristic - we found a scripting language that could be automation */
                free(process_name);
                return 2;  /* Possible automation (scripting process) */
            }

            free(process_name);
        }

        /* Get parent of current process using platform-agnostic helper */
        pid_t ppid = get_process_parent_pid(current_pid);
        if (ppid <= 1) {
            break;  /* No valid parent or reached init */
        }
        current_pid = ppid;

        depth++;
    }

    return 0;  /* No automation found in process tree */
}

/**
 * Check parent processes for Ansible indicators
 */
int check_ansible_parent_process(struct ansible_detection_info *info) {
    if (!info) {
        return 0;
    }

    /* Get our parent process ID */
    pid_t parent_pid = get_parent_process_id();
    info->parent_pid = parent_pid;

    if (parent_pid <= 1) {
        return 0;  /* No valid parent */
    }

    /* Get parent process name */
    char *parent_name = get_parent_process_name(parent_pid);
    if (parent_name) {
        strncpy(info->parent_process_name, parent_name, MAX_PROCESS_NAME_LENGTH - 1);
        info->parent_process_name[MAX_PROCESS_NAME_LENGTH - 1] = '\0';
        free(parent_name);
    } else {
        strcpy(info->parent_process_name, "unknown");
    }

    /* Walk up the process tree looking for Ansible */
    int ansible_in_tree = walk_process_tree_for_ansible(parent_pid, MAX_PROCESS_TREE_DEPTH);

    return ansible_in_tree;
}

/**
 * Check execution context for Ansible indicators
 */
int check_ansible_execution_context(struct ansible_detection_info *info) {
    if (!info) {
        return 0;
    }

    int context_score = 0;

    /* Check for SSH connection indicators */
    if (getenv("SSH_CLIENT") || getenv("SSH_CONNECTION")) {
        context_score += 10;  /* Remote execution is common with Ansible */
    }

    /* Check for non-interactive terminal */
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        context_score += 15;  /* Non-interactive execution */
    }

    /* Check for specific terminal types that suggest automation */
    const char *term = getenv("TERM");
    if (term && (strcmp(term, "dumb") == 0 || strcmp(term, "unknown") == 0)) {
        context_score += 20;  /* Automation-friendly terminal */
    }

    /* Check for Python-related environment variables that might indicate Ansible */
    if (getenv("PYTHONPATH") || getenv("PYTHONUNBUFFERED")) {
        context_score += 5;
    }

    /* Check working directory for Ansible-related paths */
    char *cwd = getcwd(NULL, 0);
    if (cwd) {
        if (strstr(cwd, "ansible") || strstr(cwd, "playbook") ||
            strstr(cwd, ".ansible")) {
            context_score += 25;
        }
        free(cwd);
    }

    return context_score;
}

/* Augment blocking functionality has been moved to ai_detection.c */

/**
 * Main Ansible detection function
 */
struct ansible_detection_info *detect_ansible_session(void) {
    struct ansible_detection_info *info = malloc(sizeof(struct ansible_detection_info));
    if (!info) {
        return NULL;
    }

    /* Initialize the structure */
    memset(info, 0, sizeof(struct ansible_detection_info));
    info->detection_time = time(NULL);
    info->method = ANSIBLE_NOT_DETECTED;
    info->confidence_level = 0;
    info->is_ansible_session = 0;

    /* Check for become method first (highest priority) */
    int become_confidence = check_ansible_become_method(info);
    if (become_confidence > 0) {
        info->confidence_level = become_confidence;
        info->is_ansible_session = 1;
        /* method and details already set by check_ansible_become_method */
    }

    /* Check environment variables */
    int env_vars_found = check_ansible_environment_variables(info);
    if (env_vars_found > 0) {
        /* Don't override become method detection unless env vars provide higher confidence */
        if (info->method == ANSIBLE_NOT_DETECTED) {
            info->method = ANSIBLE_DETECTED_ENV_VAR;
        }

        int env_confidence = (env_vars_found > 3) ? 95 : (env_vars_found * 25);
        if (env_confidence > info->confidence_level) {
            info->confidence_level = env_confidence;
            snprintf(info->detection_details, sizeof(info->detection_details),
                    "Ansible environment detected: %d variables found",
                    env_vars_found);
        }
    }

    /* Check parent processes */
    int parent_result = check_ansible_parent_process(info);
    if (parent_result > 0) {
        if (info->method == ANSIBLE_NOT_DETECTED) {
            info->method = ANSIBLE_DETECTED_PARENT_PROCESS;
        }

        int parent_confidence = (parent_result == 1) ? 90 : 60;  /* 1=definite, 2=possible */

        if (parent_confidence > info->confidence_level) {
            info->confidence_level = parent_confidence;
            snprintf(info->detection_details, sizeof(info->detection_details),
                    "Parent process indicates Ansible: %s (PID %d)",
                    info->parent_process_name, info->parent_pid);
        }
    }

    /* Check execution context */
    int context_score = check_ansible_execution_context(info);
    if (context_score > 30) {
        if (info->method == ANSIBLE_NOT_DETECTED) {
            info->method = ANSIBLE_DETECTED_CONTEXT;
        }

        int context_confidence = (context_score > 50) ? 80 : 60;
        if (context_confidence > info->confidence_level) {
            info->confidence_level = context_confidence;
            snprintf(info->detection_details, sizeof(info->detection_details),
                    "Execution context suggests Ansible (score: %d)", context_score);
        }
    }

    /* Combine multiple indicators for higher confidence */
    if (env_vars_found > 0 && parent_result > 0) {
        info->confidence_level = 98;  /* Very high confidence */
        info->method = ANSIBLE_DETECTED_ENV_VAR;  /* Primary method */
    } else if (env_vars_found > 0 && context_score > 20) {
        info->confidence_level = 90;
    } else if (parent_result > 0 && context_score > 20) {
        info->confidence_level = 85;
    }

    /* Final determination */
    info->is_ansible_session = (info->confidence_level >= 70);

    return info;
}

/**
 * Free Ansible detection info structure
 */
void free_ansible_detection_info(struct ansible_detection_info *info) {
    if (info) {
        free(info);
    }
}

/**
 * Log Ansible detection results
 */
void log_ansible_detection(const struct ansible_detection_info *info) {
    if (!info) {
        return;
    }

    const char *method_str;

    switch (info->method) {
        case ANSIBLE_DETECTED_ENV_VAR:
            method_str = "environment variables";
            break;
        case ANSIBLE_DETECTED_PARENT_PROCESS:
            method_str = "parent process";
            break;
        case ANSIBLE_DETECTED_CONTEXT:
            method_str = "execution context";
            break;
        case ANSIBLE_DETECTED_HEURISTIC:
            method_str = "heuristic analysis";
            break;
        case ANSIBLE_DETECTION_FORCED:
            method_str = "forced detection";
            break;
        default:
            method_str = "not detected";
            break;
    }

    if (info->is_ansible_session) {
        syslog(LOG_INFO, "Ansible session detected via %s (confidence: %d%%) - %s",
               method_str, info->confidence_level, info->detection_details);
    } else {
        syslog(LOG_DEBUG, "Ansible session not detected (confidence: %d%%) - %s",
               info->confidence_level, info->detection_details);
    }
}
