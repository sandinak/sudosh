/**
 * ai_detection.h - AI Tool Detection Header
 * 
 * This module provides detection capabilities for various AI tools and assistants
 * that may attempt to use sudosh for privileged operations. It's designed to
 * identify and block unauthorized AI automation while allowing legitimate tools.
 * 
 * Author: Branson Matheson <branson@sandsite.org>
 */

#ifndef AI_DETECTION_H
#define AI_DETECTION_H

#include <time.h>
#include <sys/types.h>

/* Maximum number of AI environment variables to track */
#define AI_ENV_VAR_COUNT 20

/* Maximum length for process names */
#define AI_MAX_PROCESS_NAME_LENGTH 256

/* AI tool types */
enum ai_tool_type {
    AI_TOOL_NONE = 0,
    AI_TOOL_AUGMENT,
    AI_TOOL_COPILOT,
    AI_TOOL_CHATGPT,
    AI_TOOL_CLAUDE,
    AI_TOOL_UNKNOWN
};

/* AI detection methods */
enum ai_detection_method {
    AI_NOT_DETECTED = 0,
    AI_DETECTED_ENV_VAR,
    AI_DETECTED_PARENT_PROCESS,
    AI_DETECTED_CONTEXT,
    AI_DETECTED_HEURISTIC
};

/* AI detection information structure */
struct ai_detection_info {
    enum ai_tool_type tool_type;
    enum ai_detection_method method;
    int confidence_level;           /* 0-100, higher = more confident */
    char detected_env_vars[AI_ENV_VAR_COUNT][64];
    int env_var_count;
    char parent_process_name[AI_MAX_PROCESS_NAME_LENGTH];
    pid_t parent_pid;
    char detection_details[512];    /* Human-readable detection details */
    time_t detection_time;
    int should_block;               /* Final boolean result - should this session be blocked? */
    char tool_name[32];             /* Human-readable tool name */
};

/* AI detection functions */
struct ai_detection_info *detect_ai_session(void);
void free_ai_detection_info(struct ai_detection_info *info);
int should_block_ai_session(const struct ai_detection_info *info);
void log_ai_detection(const struct ai_detection_info *info);

/* Tool-specific detection functions */
int is_augment_environment_variable(const char *var_name);
int is_copilot_environment_variable(const char *var_name);
int is_chatgpt_environment_variable(const char *var_name);
int detect_augment_session(struct ai_detection_info *info);
int detect_copilot_session(struct ai_detection_info *info);
int detect_chatgpt_session(struct ai_detection_info *info);

/* Process detection functions */
int check_ai_parent_process(struct ai_detection_info *info);
int walk_process_tree_for_ai(pid_t start_pid, int max_depth);
char *get_ai_parent_process_name(pid_t pid);

/* Utility functions */
const char *ai_tool_type_to_string(enum ai_tool_type type);
const char *ai_detection_method_to_string(enum ai_detection_method method);

#endif /* AI_DETECTION_H */
