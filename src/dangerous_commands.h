#ifndef DANGEROUS_COMMANDS_H
#define DANGEROUS_COMMANDS_H

/**
 * Dangerous Commands Detection Module
 * 
 * This module provides functionality to identify commands that should require
 * password authentication when executed in interactive editor environments
 * (VSCode, IntelliJ, Cursor, etc.) regardless of NOPASSWD settings.
 */

/**
 * Check if a command is critically dangerous
 * Critical commands always require password authentication
 * 
 * @param command The command string to check
 * @return 1 if critically dangerous, 0 otherwise
 */
int is_critical_dangerous_command(const char *command);

/**
 * Check if a command is moderately dangerous
 * Moderate commands require password in editor environments but not standard shells
 * 
 * @param command The command string to check
 * @return 1 if moderately dangerous, 0 otherwise
 */
int is_moderate_dangerous_command(const char *command);

/**
 * Check if a command involves sensitive file paths
 * Commands accessing /etc/, /var/log/, etc. require password in editors
 * 
 * @param command The command string to check
 * @return 1 if involves sensitive paths, 0 otherwise
 */
int involves_sensitive_paths(const char *command);

/**
 * Check if a command contains dangerous patterns
 * Patterns like -rf, --force, sudo, etc. require password in editors
 * 
 * @param command The command string to check
 * @return 1 if contains dangerous patterns, 0 otherwise
 */
int contains_dangerous_patterns(const char *command);

/**
 * Comprehensive check if a command should require password in editor environments
 * This is the main function that combines all danger detection methods
 * 
 * @param command The command string to check
 * @return 1 if password required in editors, 0 otherwise
 */
int requires_password_in_editor(const char *command);

/**
 * Get a human-readable explanation of why a command is dangerous
 * Useful for logging and user feedback
 * 
 * @param command The command string to explain
 * @return String explaining the danger level
 */
const char *get_danger_explanation(const char *command);

#endif /* DANGEROUS_COMMANDS_H */
