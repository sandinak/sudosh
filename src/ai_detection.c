/**
 * ai_detection.c - AI Tool Detection Implementation
 * 
 * This module implements detection capabilities for various AI tools and assistants
 * that may attempt to use sudosh for privileged operations. It provides a unified
 * interface for detecting and blocking unauthorized AI automation.
 * 
 * Author: Branson Matheson <branson@sandsite.org>
 */

#include "sudosh.h"
#include "sudosh_common.h"
#include <time.h>
#include <sys/types.h>

#include "ai_detection.h"
#include "sudosh.h"  /* For get_parent_process_id and other shared functions */

/* Environment variables that indicate Augment execution */
static const char *augment_env_vars[] = {
    "AUGMENT_SESSION_ID",
    "AUGMENT_USER_ID", 
    "AUGMENT_WORKSPACE_ID",
    "AUGMENT_TASK_ID",
    "AUGMENT_AGENT_VERSION",
    "AUGMENT_API_BASE_URL",
    "AUGMENT_EXECUTION_CONTEXT",
    "AUGMENT_CODE_CONTEXT",
    "CLAUDE_API_KEY",
    "ANTHROPIC_API_KEY",
    NULL
};

/* Environment variables that indicate GitHub Copilot execution */
static const char *copilot_env_vars[] = {
    "GITHUB_COPILOT_TOKEN",
    "GITHUB_COPILOT_API_KEY",
    "COPILOT_SESSION_ID",
    "COPILOT_USER_ID",
    "COPILOT_WORKSPACE_ID",
    "COPILOT_CHAT_SESSION",
    "COPILOT_TERMINAL_SESSION",
    "GITHUB_COPILOT_CHAT",
    "GITHUB_COPILOT_CLI",
    "GITHUB_TOKEN",
    "GH_TOKEN",
    "VSCODE_COPILOT_SESSION",
    "COPILOT_AGENT_SESSION",
    NULL
};

/* Environment variables that indicate ChatGPT/OpenAI execution */
static const char *chatgpt_env_vars[] = {
    "OPENAI_API_KEY",
    "OPENAI_API_BASE",
    "OPENAI_ORGANIZATION",
    "CHATGPT_SESSION_ID",
    "CHATGPT_USER_ID",
    "OPENAI_SESSION_TOKEN",
    "CHATGPT_API_KEY",
    NULL
};

/* AI-specific process names that indicate definite AI tool execution */
static const char *definite_ai_process_names[] = {
    /* Augment Code AI Assistant */
    "augment",
    "augment-agent",
    "augment-code",
    "augment-cli",

    /* GitHub Copilot */
    "copilot",
    "github-copilot",
    "copilot-agent",

    /* ChatGPT and OpenAI tools */
    "chatgpt",
    "openai",
    "gpt",
    "openai-cli",

    /* Claude and Anthropic tools */
    "claude",
    "anthropic",
    "claude-cli",

    /* Other AI assistants */
    "cursor",
    "codeium",
    "tabnine",
    "kite",
    "intellicode",

    NULL
};

/* Process names that might indicate AI tools but could also be legitimate user tools */
static const char *possible_ai_process_names[] = {
    /* Development environments that can run AI extensions */
    "code",
    "code-server",
    "vscode-server",
    "node",      /* VSCode extensions and other tools run in Node.js */
    "python",    /* May be running AI tools or user scripts */
    "python3",   /* May be running AI tools or user scripts */
    "npm",       /* May be installing/running AI tools or user packages */
    "yarn",      /* May be installing/running AI tools or user packages */
    "pnpm",      /* May be installing/running AI tools or user packages */

    NULL
};

/**
 * Check if an environment variable indicates Augment
 */
int is_augment_environment_variable(const char *var_name) {
    if (!var_name) {
        return 0;
    }
    
    /* Check exact matches */
    for (int i = 0; augment_env_vars[i]; i++) {
        if (strcmp(var_name, augment_env_vars[i]) == 0) {
            return 1;
        }
    }
    
    /* Check prefixes */
    if (strncmp(var_name, "AUGMENT_", 8) == 0 ||
        strncmp(var_name, "CLAUDE_", 7) == 0 ||
        strncmp(var_name, "ANTHROPIC_", 10) == 0) {
        return 1;
    }
    
    return 0;
}

/**
 * Check if an environment variable indicates GitHub Copilot
 */
int is_copilot_environment_variable(const char *var_name) {
    if (!var_name) {
        return 0;
    }
    
    /* Check exact matches */
    for (int i = 0; copilot_env_vars[i]; i++) {
        if (strcmp(var_name, copilot_env_vars[i]) == 0) {
            return 1;
        }
    }
    
    /* Check prefixes */
    if (strncmp(var_name, "COPILOT_", 8) == 0 ||
        strncmp(var_name, "GITHUB_COPILOT_", 15) == 0 ||
        strncmp(var_name, "VSCODE_COPILOT_", 15) == 0) {
        return 1;
    }
    
    return 0;
}

/**
 * Check if an environment variable indicates ChatGPT/OpenAI
 */
int is_chatgpt_environment_variable(const char *var_name) {
    if (!var_name) {
        return 0;
    }
    
    /* Check exact matches */
    for (int i = 0; chatgpt_env_vars[i]; i++) {
        if (strcmp(var_name, chatgpt_env_vars[i]) == 0) {
            return 1;
        }
    }
    
    /* Check prefixes */
    if (strncmp(var_name, "OPENAI_", 7) == 0 ||
        strncmp(var_name, "CHATGPT_", 8) == 0) {
        return 1;
    }
    
    return 0;
}

/**
 * Detect Augment session
 */
int detect_augment_session(struct ai_detection_info *info) {
    if (!info) {
        return 0;
    }
    
    int augment_vars_found = 0;
    
    /* Check environment variables */
    extern char **environ;
    for (char **env = environ; *env && info->env_var_count < AI_ENV_VAR_COUNT; env++) {
        char *var_copy = strdup(*env);
        if (!var_copy) continue;
        
        char *equals = strchr(var_copy, '=');
        if (equals) {
            *equals = '\0';
            
            if (is_augment_environment_variable(var_copy)) {
                strncpy(info->detected_env_vars[info->env_var_count], 
                       var_copy, 63);
                info->detected_env_vars[info->env_var_count][63] = '\0';
                info->env_var_count++;
                augment_vars_found++;
            }
        }
        
        free(var_copy);
    }
    
    if (augment_vars_found > 0) {
        info->tool_type = AI_TOOL_AUGMENT;
        info->method = AI_DETECTED_ENV_VAR;
        info->confidence_level = (augment_vars_found >= 3) ? 95 : (augment_vars_found * 30);
        info->should_block = 1;  /* Always block Augment */
        strcpy(info->tool_name, "Augment");
        snprintf(info->detection_details, sizeof(info->detection_details),
                "Augment environment detected: %d variables found", augment_vars_found);
        return 1;
    }
    
    return 0;
}

/**
 * Detect GitHub Copilot session
 */
int detect_copilot_session(struct ai_detection_info *info) {
    if (!info) {
        return 0;
    }
    
    int copilot_vars_found = 0;
    
    /* Check environment variables */
    extern char **environ;
    for (char **env = environ; *env && info->env_var_count < AI_ENV_VAR_COUNT; env++) {
        char *var_copy = strdup(*env);
        if (!var_copy) continue;
        
        char *equals = strchr(var_copy, '=');
        if (equals) {
            *equals = '\0';
            
            if (is_copilot_environment_variable(var_copy)) {
                strncpy(info->detected_env_vars[info->env_var_count], 
                       var_copy, 63);
                info->detected_env_vars[info->env_var_count][63] = '\0';
                info->env_var_count++;
                copilot_vars_found++;
            }
        }
        
        free(var_copy);
    }
    
    if (copilot_vars_found > 0) {
        info->tool_type = AI_TOOL_COPILOT;
        info->method = AI_DETECTED_ENV_VAR;
        info->confidence_level = (copilot_vars_found >= 2) ? 90 : (copilot_vars_found * 40);
        info->should_block = 1;  /* Block Copilot by default */
        strcpy(info->tool_name, "GitHub Copilot");
        snprintf(info->detection_details, sizeof(info->detection_details),
                "GitHub Copilot environment detected: %d variables found", copilot_vars_found);
        return 1;
    }
    
    return 0;
}

/**
 * Detect ChatGPT/OpenAI session
 */
int detect_chatgpt_session(struct ai_detection_info *info) {
    if (!info) {
        return 0;
    }
    
    int chatgpt_vars_found = 0;
    
    /* Check environment variables */
    extern char **environ;
    for (char **env = environ; *env && info->env_var_count < AI_ENV_VAR_COUNT; env++) {
        char *var_copy = strdup(*env);
        if (!var_copy) continue;
        
        char *equals = strchr(var_copy, '=');
        if (equals) {
            *equals = '\0';
            
            if (is_chatgpt_environment_variable(var_copy)) {
                strncpy(info->detected_env_vars[info->env_var_count], 
                       var_copy, 63);
                info->detected_env_vars[info->env_var_count][63] = '\0';
                info->env_var_count++;
                chatgpt_vars_found++;
            }
        }
        
        free(var_copy);
    }
    
    if (chatgpt_vars_found > 0) {
        info->tool_type = AI_TOOL_CHATGPT;
        info->method = AI_DETECTED_ENV_VAR;
        info->confidence_level = (chatgpt_vars_found >= 2) ? 85 : (chatgpt_vars_found * 35);
        info->should_block = 1;  /* Block ChatGPT by default */
        strcpy(info->tool_name, "ChatGPT/OpenAI");
        snprintf(info->detection_details, sizeof(info->detection_details),
                "ChatGPT/OpenAI environment detected: %d variables found", chatgpt_vars_found);
        return 1;
    }
    
    return 0;
}

/**
 * Check if the current command appears to be a legitimate administrative operation
 * Returns: 1 if legitimate admin operation, 0 otherwise
 */
static int is_legitimate_admin_operation(void) {
    /* Get command line arguments if available */
    char cmdline[1024] = {0};
    FILE *cmdline_file = fopen("/proc/self/cmdline", "r");
    if (cmdline_file) {
        size_t bytes_read = fread(cmdline, 1, sizeof(cmdline) - 1, cmdline_file);
        fclose(cmdline_file);

        /* Replace null bytes with spaces for easier parsing */
        for (size_t i = 0; i < bytes_read; i++) {
            if (cmdline[i] == '\0') {
                cmdline[i] = ' ';
            }
        }

        /* Check for common legitimate admin operations */
        if (strstr(cmdline, "make install") ||
            strstr(cmdline, "make clean") ||
            strstr(cmdline, "install") ||
            strstr(cmdline, "systemctl") ||
            strstr(cmdline, "service") ||
            strstr(cmdline, "apt") ||
            strstr(cmdline, "yum") ||
            strstr(cmdline, "dnf") ||
            strstr(cmdline, "rpm")) {
            return 1;  /* Legitimate admin operation */
        }
    }

    return 0;  /* Not a recognized admin operation */
}

/**
 * Main AI detection function
 */
struct ai_detection_info *detect_ai_session(void) {
    struct ai_detection_info *info = malloc(sizeof(struct ai_detection_info));
    if (!info) {
        return NULL;
    }

    /* Initialize the structure */
    memset(info, 0, sizeof(struct ai_detection_info));
    info->detection_time = time(NULL);
    info->tool_type = AI_TOOL_NONE;
    info->method = AI_NOT_DETECTED;
    info->confidence_level = 0;
    info->should_block = 0;

    /* Try to detect different AI tools in order of priority */
    /* Augment has highest priority for blocking */
    if (detect_augment_session(info)) {
        return info;
    }

    /* Reset for next detection */
    info->env_var_count = 0;
    memset(info->detected_env_vars, 0, sizeof(info->detected_env_vars));

    /* Check for Copilot */
    if (detect_copilot_session(info)) {
        return info;
    }

    /* Reset for next detection */
    info->env_var_count = 0;
    memset(info->detected_env_vars, 0, sizeof(info->detected_env_vars));

    /* Check for ChatGPT */
    if (detect_chatgpt_session(info)) {
        return info;
    }

    /* Reset for process detection */
    info->env_var_count = 0;
    memset(info->detected_env_vars, 0, sizeof(info->detected_env_vars));

    /* If no environment variables detected AI, try process detection */
    int process_confidence = check_ai_parent_process(info);
    if (process_confidence > 0) {
        info->confidence_level = process_confidence;

        /* Check if this is a legitimate admin operation */
        int is_admin_op = is_legitimate_admin_operation();

        /* Determine if we should block based on confidence and operation type */
        if (process_confidence >= 85 && !is_admin_op) {
            info->should_block = 1;  /* Only block with very high confidence and not admin ops */
        } else if (process_confidence >= 95) {
            info->should_block = 1;  /* Block very high confidence regardless */
        }

        /* Set tool name based on what we found - only if tool_type was actually set by detection */
        if (info->tool_type == AI_TOOL_NONE && process_confidence >= 85) {
            info->tool_type = AI_TOOL_AUGMENT;  /* Default to Augment for high-confidence process detection */
        }

        switch (info->tool_type) {
            case AI_TOOL_AUGMENT:
                strcpy(info->tool_name, "Augment");
                break;
            case AI_TOOL_COPILOT:
                strcpy(info->tool_name, "GitHub Copilot");
                break;
            case AI_TOOL_CHATGPT:
                strcpy(info->tool_name, "ChatGPT");
                break;
            default:
                strcpy(info->tool_name, "Unknown AI Tool");
                break;
        }

        return info;
    }

    /* No AI tool detected */
    strcpy(info->tool_name, "None");
    strcpy(info->detection_details, "No AI tool detected");

    return info;
}

/**
 * Check if AI session should be blocked
 */
int should_block_ai_session(const struct ai_detection_info *info) {
    if (!info) {
        return 0;
    }

    return info->should_block;
}

/**
 * Free AI detection info structure
 */
void free_ai_detection_info(struct ai_detection_info *info) {
    if (info) {
        free(info);
    }
}

/**
 * Log AI detection results
 */
void log_ai_detection(const struct ai_detection_info *info) {
    if (!info) {
        return;
    }

    const char *method_str = ai_detection_method_to_string(info->method);
    const char *tool_str = ai_tool_type_to_string(info->tool_type);

    if (info->should_block) {
        syslog(LOG_WARNING, "AI_BLOCKED: %s session detected and blocked via %s (confidence: %d%%) - %s",
               tool_str, method_str, info->confidence_level, info->detection_details);
    } else if (info->tool_type != AI_TOOL_NONE) {
        syslog(LOG_INFO, "AI_DETECTED: %s session detected via %s (confidence: %d%%) - %s",
               tool_str, method_str, info->confidence_level, info->detection_details);
    } else {
        syslog(LOG_DEBUG, "AI_DETECTION: No AI tool detected");
    }
}

/**
 * Convert AI tool type to string
 */
const char *ai_tool_type_to_string(enum ai_tool_type type) {
    switch (type) {
        case AI_TOOL_AUGMENT:
            return "Augment";
        case AI_TOOL_COPILOT:
            return "GitHub Copilot";
        case AI_TOOL_CHATGPT:
            return "ChatGPT/OpenAI";
        case AI_TOOL_CLAUDE:
            return "Claude";
        case AI_TOOL_UNKNOWN:
            return "Unknown AI";
        case AI_TOOL_NONE:
        default:
            return "None";
    }
}

/**
 * Convert AI detection method to string
 */
const char *ai_detection_method_to_string(enum ai_detection_method method) {
    switch (method) {
        case AI_DETECTED_ENV_VAR:
            return "environment variables";
        case AI_DETECTED_PARENT_PROCESS:
            return "parent process";
        case AI_DETECTED_CONTEXT:
            return "execution context";
        case AI_DETECTED_HEURISTIC:
            return "heuristic analysis";
        case AI_NOT_DETECTED:
        default:
            return "not detected";
    }
}

/**
 * Get the parent process ID for a given process (AI detection helper)
 * This is a simplified version that works across platforms
 */
static pid_t get_ai_process_parent_pid(pid_t pid) {
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
    /* Fallback for non-Linux systems - only works for current process */
    if (pid == getpid()) {
        return getppid();
    }
    return 0;
#endif
}

/**
 * Get the process name for a given PID (AI-specific version)
 * This reuses the existing get_parent_process_name function from ansible_detection.c
 */
char *get_ai_parent_process_name(pid_t pid) {
    return get_parent_process_name(pid);
}

/**
 * Check if the current execution context suggests AI automation vs user interaction
 * Returns: 0 = likely user, 1 = likely AI automation, 2 = uncertain
 */
static int analyze_execution_context(void) {
    int ai_indicators = 0;
    int user_indicators = 0;

    /* Check if we have a controlling terminal */
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && isatty(STDERR_FILENO)) {
        user_indicators += 25;  /* Interactive terminal strongly suggests user */
    } else {
        ai_indicators += 10;    /* Non-interactive suggests automation, but not definitive */
    }

    /* Check terminal type */
    const char *term = getenv("TERM");
    if (term) {
        if (strcmp(term, "xterm-256color") == 0 ||
            strcmp(term, "screen") == 0 ||
            strcmp(term, "tmux") == 0 ||
            strcmp(term, "xterm") == 0) {
            user_indicators += 15;  /* Common interactive terminals */
        } else if (strcmp(term, "dumb") == 0 || strcmp(term, "unknown") == 0) {
            ai_indicators += 25;    /* Automation-friendly terminals */
        }
    }

    /* Check for specific AI automation indicators */
    if (getenv("AUGMENT_SESSION_ID") || getenv("AUGMENT_USER_ID") ||
        getenv("CLAUDE_API_KEY") || getenv("ANTHROPIC_API_KEY") ||
        getenv("GITHUB_COPILOT_TOKEN") || getenv("OPENAI_API_KEY")) {
        ai_indicators += 50;        /* Strong AI indicators */
    }

    /* Check for SSH connection (could be user or automation) */
    if (getenv("SSH_CLIENT") || getenv("SSH_CONNECTION")) {
        /* SSH could be either, neutral indicator */
        user_indicators += 2;
    }

    /* Check for shell level - but be more lenient */
    const char *shlvl = getenv("SHLVL");
    if (shlvl) {
        int level = atoi(shlvl);
        if (level > 5) {
            ai_indicators += 15;    /* Very deep shell nesting suggests automation */
        } else if (level == 1 || level == 2) {
            user_indicators += 8;   /* Normal shell levels suggest user */
        }
        /* Levels 3-5 are neutral - common in development environments */
    }

    /* Check for automation-specific environment variables */
    if (getenv("CI") || getenv("JENKINS_URL") || getenv("GITHUB_ACTIONS") ||
        getenv("GITLAB_CI") || getenv("TRAVIS") || getenv("CIRCLECI")) {
        ai_indicators += 30;        /* CI/CD environment */
    }

    /* Check for user-specific environment variables */
    if (getenv("USER") && getenv("HOME") && getenv("SHELL")) {
        user_indicators += 10;      /* Normal user environment */
    }

    /* Check for development environment indicators that suggest user activity */
    if (getenv("PWD") && getenv("OLDPWD")) {
        user_indicators += 5;       /* Normal shell navigation */
    }

    /* Check for VSCode-specific environment that might indicate user vs AI */
    if (getenv("VSCODE_PID") || getenv("VSCODE_IPC_HOOK_CLI")) {
        /* VSCode environment - could be user or AI, but lean towards user */
        user_indicators += 8;
    }

    /* Check process timing - rapid successive calls might indicate automation */
    static time_t last_check_time = 0;
    time_t current_time = time(NULL);
    if (last_check_time > 0 && (current_time - last_check_time) < 2) {
        ai_indicators += 10;        /* Rapid successive calls suggest automation */
    }
    last_check_time = current_time;

    /* Determine result based on indicators with higher threshold for blocking */
    if (ai_indicators > user_indicators + 20) {
        return 1;  /* Likely AI automation - need strong evidence */
    } else if (user_indicators > ai_indicators + 5) {
        return 0;  /* Likely user */
    } else {
        return 2;  /* Uncertain - default to allowing */
    }
}

/**
 * Walk up the process tree looking for AI tool processes
 * Returns: 0 = no AI found, 1 = definite AI tool, 2 = possible AI tool, 3 = development environment
 */
int walk_process_tree_for_ai(pid_t start_pid, int max_depth) {
    pid_t current_pid = start_pid;
    int depth = 0;
    int found_development_env = 0;

    while (current_pid > 1 && depth < max_depth) {
        char *process_name = get_ai_parent_process_name(current_pid);
        if (process_name) {
            /* Check for definite AI tools first */
            for (int i = 0; definite_ai_process_names[i]; i++) {
                if (strcmp(process_name, definite_ai_process_names[i]) == 0) {
                    free(process_name);
                    return 1;  /* Found definite AI tool in process tree */
                }
            }

            /* Check for possible AI tools (development environments) */
            for (int i = 0; possible_ai_process_names[i]; i++) {
                if (strcmp(process_name, possible_ai_process_names[i]) == 0) {
                    found_development_env = 1;
                    break;
                }
            }

            /* Special case for VSCode-related processes */
            if (strstr(process_name, "vscode") || strstr(process_name, "code-server")) {
                found_development_env = 1;
            }

            free(process_name);
        }

        /* Get parent of current process using our AI detection helper function */
        pid_t ppid = get_ai_process_parent_pid(current_pid);
        if (ppid <= 1) {
            break;  /* No valid parent or reached init */
        }
        current_pid = ppid;

        depth++;
    }

    if (found_development_env) {
        return 3;  /* Found development environment (VSCode, Node.js, etc.) */
    }

    return 0;  /* No AI tools found in process tree */
}

/**
 * Check parent processes for AI tool indicators
 * Returns confidence level (0-100) based on what's found
 */
int check_ai_parent_process(struct ai_detection_info *info) {
    if (!info) {
        return 0;
    }

    /* Get our parent process ID */
    pid_t parent_pid = get_parent_process_id();
    if (parent_pid <= 1) {
        return 0;  /* No valid parent */
    }

    /* Analyze execution context first */
    int context_analysis = analyze_execution_context();

    /* Get parent process name */
    char *parent_name = get_ai_parent_process_name(parent_pid);
    if (parent_name) {
        /* Store parent process info */
        snprintf(info->detection_details, sizeof(info->detection_details),
                "Parent process: %s (PID %d), Context: %s", parent_name, parent_pid,
                (context_analysis == 0) ? "user" : (context_analysis == 1) ? "automation" : "uncertain");

        /* Check if parent process is a definite AI tool */
        for (int i = 0; definite_ai_process_names[i]; i++) {
            if (strcmp(parent_name, definite_ai_process_names[i]) == 0) {
                info->tool_type = AI_TOOL_AUGMENT;
                info->method = AI_DETECTED_PARENT_PROCESS;
                free(parent_name);
                return 95;  /* Very high confidence for definite AI tools */
            }
        }

        /* Check if parent process is a possible AI tool (development environment) */
        for (int i = 0; possible_ai_process_names[i]; i++) {
            if (strcmp(parent_name, possible_ai_process_names[i]) == 0 ||
                strstr(parent_name, "vscode") != NULL) {

                /* Found development environment - use context analysis */
                if (context_analysis == 1) {
                    /* Automation context in development environment = likely AI */
                    info->tool_type = AI_TOOL_AUGMENT;
                    info->method = AI_DETECTED_CONTEXT;
                    free(parent_name);
                    return 85;  /* High confidence for automation in dev env */
                } else if (context_analysis == 0) {
                    /* User context in development environment = likely legitimate */
                    free(parent_name);
                    return 0;   /* Don't block user operations */
                } else {
                    /* Uncertain context - use lower confidence */
                    info->tool_type = AI_TOOL_AUGMENT;
                    info->method = AI_DETECTED_HEURISTIC;
                    free(parent_name);
                    return 45;  /* Lower confidence - won't trigger blocking */
                }
            }
        }
        free(parent_name);
    }

    /* Walk up the process tree looking for AI tools */
    int ai_in_tree = walk_process_tree_for_ai(parent_pid, MAX_PROCESS_TREE_DEPTH);
    if (ai_in_tree > 0) {
        info->method = AI_DETECTED_PARENT_PROCESS;

        if (ai_in_tree == 1) {
            /* Definite AI tool found */
            info->tool_type = AI_TOOL_AUGMENT;
            return 90;  /* High confidence */
        } else if (ai_in_tree == 3) {
            /* Development environment found - use context analysis */
            if (context_analysis == 1) {
                /* Automation context = likely AI */
                info->tool_type = AI_TOOL_AUGMENT;
                info->method = AI_DETECTED_CONTEXT;
                return 80;  /* High confidence for automation */
            } else if (context_analysis == 0) {
                /* User context = likely legitimate */
                return 0;   /* Don't block user operations */
            } else {
                /* Uncertain context */
                info->tool_type = AI_TOOL_AUGMENT;
                info->method = AI_DETECTED_HEURISTIC;
                return 40;  /* Lower confidence - won't trigger blocking */
            }
        }
    }

    return 0;  /* No AI tools found in process tree */
}
