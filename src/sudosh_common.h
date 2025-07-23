/**
 * sudosh_common.h - Common utilities and error handling framework
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * This file provides common utilities, error handling macros, and
 * memory management functions to reduce code duplication across
 * the sudosh codebase.
 */

#ifndef SUDOSH_COMMON_H
#define SUDOSH_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <ctype.h>

/* Error codes */
typedef enum {
    SUDOSH_SUCCESS = 0,
    SUDOSH_ERROR_NULL_POINTER = -1,
    SUDOSH_ERROR_MEMORY_ALLOCATION = -2,
    SUDOSH_ERROR_INVALID_INPUT = -3,
    SUDOSH_ERROR_AUTHENTICATION_FAILED = -4,
    SUDOSH_ERROR_PERMISSION_DENIED = -5,
    SUDOSH_ERROR_FILE_NOT_FOUND = -6,
    SUDOSH_ERROR_SYSTEM_ERROR = -7,
    SUDOSH_ERROR_BUFFER_OVERFLOW = -8,
    SUDOSH_ERROR_INVALID_CONFIGURATION = -9,
    SUDOSH_ERROR_TIMEOUT = -10
} sudosh_error_t;

/* Error handling macros */
#define SUDOSH_CHECK_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            log_error(msg); \
            return SUDOSH_ERROR_NULL_POINTER; \
        } \
    } while(0)

#define SUDOSH_CHECK_NULL_RETURN_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            log_error(msg); \
            return NULL; \
        } \
    } while(0)

#define SUDOSH_CHECK_ALLOC(ptr) \
    SUDOSH_CHECK_NULL(ptr, "Memory allocation failed")

#define SUDOSH_CHECK_ALLOC_RETURN_NULL(ptr) \
    SUDOSH_CHECK_NULL_RETURN_NULL(ptr, "Memory allocation failed")

#define SUDOSH_RETURN_ON_ERROR(result) \
    do { \
        sudosh_error_t _err = (result); \
        if (_err != SUDOSH_SUCCESS) { \
            return _err; \
        } \
    } while(0)

#define SUDOSH_LOG_AND_RETURN_ERROR(error_code, msg) \
    do { \
        log_error(msg); \
        return (error_code); \
    } while(0)

/* Forward declarations */
void log_error(const char *message);

/* Memory management utilities */

/**
 * Safe string duplication with null checking
 */
static inline char *sudosh_safe_strdup(const char *str) {
    if (!str) {
        return NULL;
    }

    char *copy = malloc(strlen(str) + 1);
    if (!copy) {
        syslog(LOG_ERR, "sudosh: Memory allocation failed in sudosh_safe_strdup");
        return NULL;
    }

    strcpy(copy, str);
    return copy;
}

/**
 * Safe memory allocation with zero initialization
 */
static inline void *sudosh_safe_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    void *ptr = malloc(size);
    if (!ptr) {
        syslog(LOG_ERR, "sudosh: Memory allocation failed in sudosh_safe_malloc");
        return NULL;
    }

    memset(ptr, 0, size);
    return ptr;
}

/**
 * Safe memory reallocation
 */
static inline void *sudosh_safe_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        syslog(LOG_ERR, "sudosh: Memory reallocation failed in sudosh_safe_realloc");
        /* Original pointer is still valid */
        return NULL;
    }

    return new_ptr;
}

/**
 * Safe memory free with pointer nullification
 */
static inline void sudosh_safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* String utilities */

/**
 * Safe string copy with bounds checking
 */
static inline sudosh_error_t sudosh_safe_strcpy(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) {
        return SUDOSH_ERROR_BUFFER_OVERFLOW;
    }
    
    strcpy(dest, src);
    return SUDOSH_SUCCESS;
}

/**
 * Safe string concatenation with bounds checking
 */
static inline sudosh_error_t sudosh_safe_strcat(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        return SUDOSH_ERROR_BUFFER_OVERFLOW;
    }
    
    strcat(dest, src);
    return SUDOSH_SUCCESS;
}

/**
 * Safe string formatting with bounds checking
 */
static inline sudosh_error_t sudosh_safe_snprintf(char *dest, size_t dest_size, const char *format, ...) {
    if (!dest || !format || dest_size == 0) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, dest_size, format, args);
    va_end(args);
    
    if (result < 0 || (size_t)result >= dest_size) {
        return SUDOSH_ERROR_BUFFER_OVERFLOW;
    }
    
    return SUDOSH_SUCCESS;
}

/* Input validation utilities */

/**
 * Validate string length
 */
static inline sudosh_error_t sudosh_validate_string_length(const char *str, size_t max_length) {
    if (!str) {
        return SUDOSH_ERROR_NULL_POINTER;
    }
    
    if (strlen(str) > max_length) {
        return SUDOSH_ERROR_INVALID_INPUT;
    }
    
    return SUDOSH_SUCCESS;
}

/**
 * Validate string contains only safe characters
 */
static inline sudosh_error_t sudosh_validate_safe_string(const char *str) {
    if (!str) {
        return SUDOSH_ERROR_NULL_POINTER;
    }
    
    for (const char *p = str; *p; p++) {
        if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.' && *p != '/' && *p != ' ') {
            return SUDOSH_ERROR_INVALID_INPUT;
        }
    }
    
    return SUDOSH_SUCCESS;
}

/* Logging utilities */

/**
 * Log error with consistent formatting
 */
static inline void sudosh_log_error(const char *function, const char *message) {
    if (function && message) {
        syslog(LOG_ERR, "sudosh[%s]: %s", function, message);
    }
}

/**
 * Log warning with consistent formatting
 */
static inline void sudosh_log_warning(const char *function, const char *message) {
    if (function && message) {
        syslog(LOG_WARNING, "sudosh[%s]: %s", function, message);
    }
}

/**
 * Log info with consistent formatting
 */
static inline void sudosh_log_info(const char *function, const char *message) {
    if (function && message) {
        syslog(LOG_INFO, "sudosh[%s]: %s", function, message);
    }
}

/* Convenience macros for logging with function name */
#define SUDOSH_LOG_ERROR(msg) sudosh_log_error(__func__, msg)
#define SUDOSH_LOG_WARNING(msg) sudosh_log_warning(__func__, msg)
#define SUDOSH_LOG_INFO(msg) sudosh_log_info(__func__, msg)

/* Configuration structure */
typedef struct {
    int auth_cache_timeout;
    int inactivity_timeout;
    int max_command_length;
    int verbose_mode;
    int test_mode;
    char *log_facility;
    char *cache_directory;
    char *lock_directory;

    /* Ansible detection configuration */
    int ansible_detection_enabled;
    int ansible_detection_force;
    int ansible_detection_verbose;
    int ansible_detection_confidence_threshold;
} sudosh_config_t;

/* Configuration management functions */
sudosh_config_t *sudosh_config_init(void);
void sudosh_config_free(sudosh_config_t *config);
sudosh_error_t sudosh_config_load(sudosh_config_t *config, const char *config_file);
sudosh_error_t sudosh_config_validate(const sudosh_config_t *config);

#endif /* SUDOSH_COMMON_H */
