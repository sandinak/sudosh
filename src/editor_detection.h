#ifndef EDITOR_DETECTION_H
#define EDITOR_DETECTION_H

/**
 * Editor Environment Detection Module
 * 
 * This module provides functionality to detect when sudosh is running
 * within interactive editor environments like VSCode, IntelliJ, Cursor, etc.
 * This is used to determine when enhanced authentication should be required.
 */

/* Structure to hold editor detection information */
struct editor_detection_info {
    int is_editor_environment;      /* Overall result: 1 if editor detected, 0 otherwise */
    int confidence_level;           /* Confidence level 0-100 */
    int has_env_vars;              /* 1 if editor environment variables detected */
    int has_editor_process;        /* 1 if editor process found in process tree */
    int is_remote_session;         /* 1 if remote/SSH editor session */
    int has_gui_session;           /* 1 if GUI session detected */
    char detection_details[256];    /* Human-readable detection details */
};

/**
 * Check if current environment has editor-specific environment variables
 * 
 * @return 1 if editor environment variables found, 0 otherwise
 */
int has_editor_environment_variables(void);

/**
 * Check if current terminal type suggests an editor environment
 * 
 * @return 1 if editor terminal type detected, 0 otherwise
 */
int has_editor_terminal_type(void);

/**
 * Walk up the process tree looking for editor processes
 * 
 * @return 1 if editor process found in tree, 0 otherwise
 */
int has_editor_in_process_tree(void);

/**
 * Check if we're running in a remote/SSH editor session
 * 
 * @return 1 if remote editor session detected, 0 otherwise
 */
int is_remote_editor_session(void);

/**
 * Main function to detect if we're running in an interactive editor environment
 * This combines all detection methods to make a final determination
 * 
 * @return 1 if interactive editor environment detected, 0 otherwise
 */
int is_interactive_editor_environment(void);

/**
 * Get detailed information about the detected editor environment
 * Caller is responsible for freeing the returned structure
 * 
 * @return Pointer to editor detection info structure, or NULL on error
 */
struct editor_detection_info *get_editor_detection_info(void);

#endif /* EDITOR_DETECTION_H */
