/**
 * config.c - Configuration Management
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * Centralized configuration management for sudosh with support for
 * default values, file-based configuration, and runtime validation.
 */

#include "sudosh.h"
#include "sudosh_common.h"

/* Default configuration values */
#define DEFAULT_AUTH_CACHE_TIMEOUT 900
#define DEFAULT_INACTIVITY_TIMEOUT 300
#define DEFAULT_MAX_COMMAND_LENGTH 4096
#define DEFAULT_LOG_FACILITY "authpriv"
#define DEFAULT_CACHE_DIRECTORY "/var/run/sudosh"
#define DEFAULT_LOCK_DIRECTORY "/var/run/sudosh/locks"

/**
 * Initialize configuration with default values
 */
sudosh_config_t *sudosh_config_init(void) {
    sudosh_config_t *config = sudosh_safe_malloc(sizeof(sudosh_config_t));
    if (!config) {
        return NULL;
    }

    /* Set default values */
    config->auth_cache_timeout = DEFAULT_AUTH_CACHE_TIMEOUT;
    config->inactivity_timeout = DEFAULT_INACTIVITY_TIMEOUT;
    config->max_command_length = DEFAULT_MAX_COMMAND_LENGTH;
    config->verbose_mode = 0;
    config->test_mode = 0;
    
    config->log_facility = sudosh_safe_strdup(DEFAULT_LOG_FACILITY);
    config->cache_directory = sudosh_safe_strdup(DEFAULT_CACHE_DIRECTORY);
    config->lock_directory = sudosh_safe_strdup(DEFAULT_LOCK_DIRECTORY);
    
    if (!config->log_facility || !config->cache_directory || !config->lock_directory) {
        sudosh_config_free(config);
        return NULL;
    }

    return config;
}

/**
 * Free configuration structure
 */
void sudosh_config_free(sudosh_config_t *config) {
    if (!config) {
        return;
    }

    sudosh_safe_free((void**)&config->log_facility);
    sudosh_safe_free((void**)&config->cache_directory);
    sudosh_safe_free((void**)&config->lock_directory);
    
    sudosh_safe_free((void**)&config);
}

/**
 * Parse a configuration line
 */
static sudosh_error_t parse_config_line(sudosh_config_t *config, const char *line) {
    if (!config || !line) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }

    /* Skip empty lines and comments */
    if (line[0] == '\0' || line[0] == '#' || line[0] == '\n') {
        return SUDOSH_SUCCESS;
    }

    char key[256], value[256];
    if (sscanf(line, "%255s = %255s", key, value) != 2) {
        /* Try without spaces around = */
        if (sscanf(line, "%255s=%255s", key, value) != 2) {
            return SUDOSH_ERROR_INVALID_CONFIGURATION;
        }
    }

    /* Parse configuration options */
    if (strcmp(key, "auth_cache_timeout") == 0) {
        config->auth_cache_timeout = atoi(value);
    } else if (strcmp(key, "inactivity_timeout") == 0) {
        config->inactivity_timeout = atoi(value);
    } else if (strcmp(key, "max_command_length") == 0) {
        config->max_command_length = atoi(value);
    } else if (strcmp(key, "verbose_mode") == 0) {
        config->verbose_mode = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "test_mode") == 0) {
        config->test_mode = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    } else if (strcmp(key, "log_facility") == 0) {
        sudosh_safe_free((void**)&config->log_facility);
        config->log_facility = sudosh_safe_strdup(value);
        if (!config->log_facility) {
            return SUDOSH_ERROR_MEMORY_ALLOCATION;
        }
    } else if (strcmp(key, "cache_directory") == 0) {
        sudosh_safe_free((void**)&config->cache_directory);
        config->cache_directory = sudosh_safe_strdup(value);
        if (!config->cache_directory) {
            return SUDOSH_ERROR_MEMORY_ALLOCATION;
        }
    } else if (strcmp(key, "lock_directory") == 0) {
        sudosh_safe_free((void**)&config->lock_directory);
        config->lock_directory = sudosh_safe_strdup(value);
        if (!config->lock_directory) {
            return SUDOSH_ERROR_MEMORY_ALLOCATION;
        }
    } else {
        /* Unknown configuration option - log warning but continue */
        char warning_msg[512];
        snprintf(warning_msg, sizeof(warning_msg), "Unknown configuration option: %s", key);
        SUDOSH_LOG_WARNING(warning_msg);
    }

    return SUDOSH_SUCCESS;
}

/**
 * Load configuration from file
 */
sudosh_error_t sudosh_config_load(sudosh_config_t *config, const char *config_file) {
    if (!config || !config_file) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }

    FILE *file = fopen(config_file, "r");
    if (!file) {
        /* Configuration file is optional */
        SUDOSH_LOG_INFO("Configuration file not found, using defaults");
        return SUDOSH_SUCCESS;
    }

    char line[1024];
    int line_number = 0;
    sudosh_error_t result = SUDOSH_SUCCESS;

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        sudosh_error_t parse_result = parse_config_line(config, line);
        if (parse_result != SUDOSH_SUCCESS) {
            char error_msg[1024];
            int written = snprintf(error_msg, sizeof(error_msg),
                    "Configuration error at line %d: %.256s", line_number, line);
            if (written >= (int)sizeof(error_msg)) {
                snprintf(error_msg, sizeof(error_msg),
                        "Configuration error at line %d: [line too long]", line_number);
            }
            SUDOSH_LOG_ERROR(error_msg);
            result = parse_result;
        }
    }

    fclose(file);
    return result;
}

/**
 * Validate configuration values
 */
sudosh_error_t sudosh_config_validate(const sudosh_config_t *config) {
    if (!config) {
        return SUDOSH_ERROR_NULL_POINTER;
    }

    /* Validate timeout values */
    if (config->auth_cache_timeout < 0 || config->auth_cache_timeout > 86400) {
        SUDOSH_LOG_ERROR("Invalid auth_cache_timeout (must be 0-86400 seconds)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    if (config->inactivity_timeout < 0 || config->inactivity_timeout > 86400) {
        SUDOSH_LOG_ERROR("Invalid inactivity_timeout (must be 0-86400 seconds)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    /* Validate command length */
    if (config->max_command_length < 256 || config->max_command_length > 65536) {
        SUDOSH_LOG_ERROR("Invalid max_command_length (must be 256-65536 characters)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    /* Validate required string fields */
    if (!config->log_facility || strlen(config->log_facility) == 0) {
        SUDOSH_LOG_ERROR("Invalid log_facility (cannot be empty)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    if (!config->cache_directory || strlen(config->cache_directory) == 0) {
        SUDOSH_LOG_ERROR("Invalid cache_directory (cannot be empty)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    if (!config->lock_directory || strlen(config->lock_directory) == 0) {
        SUDOSH_LOG_ERROR("Invalid lock_directory (cannot be empty)");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    /* Validate directory paths */
    if (config->cache_directory[0] != '/') {
        SUDOSH_LOG_ERROR("cache_directory must be an absolute path");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    if (config->lock_directory[0] != '/') {
        SUDOSH_LOG_ERROR("lock_directory must be an absolute path");
        return SUDOSH_ERROR_INVALID_CONFIGURATION;
    }

    return SUDOSH_SUCCESS;
}

/**
 * Get configuration value as string for debugging
 */
char *sudosh_config_to_string(const sudosh_config_t *config) {
    if (!config) {
        return NULL;
    }

    char *result = sudosh_safe_malloc(2048);
    if (!result) {
        return NULL;
    }

    snprintf(result, 2048,
        "Sudosh Configuration:\n"
        "  auth_cache_timeout: %d\n"
        "  inactivity_timeout: %d\n"
        "  max_command_length: %d\n"
        "  verbose_mode: %s\n"
        "  test_mode: %s\n"
        "  log_facility: %s\n"
        "  cache_directory: %s\n"
        "  lock_directory: %s\n",
        config->auth_cache_timeout,
        config->inactivity_timeout,
        config->max_command_length,
        config->verbose_mode ? "true" : "false",
        config->test_mode ? "true" : "false",
        config->log_facility ? config->log_facility : "NULL",
        config->cache_directory ? config->cache_directory : "NULL",
        config->lock_directory ? config->lock_directory : "NULL"
    );

    return result;
}
