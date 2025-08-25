/**
 * path_validator.c - Standalone PATH Security Validation Tool
 *
 * Author: Branson Matheson <branson@sandsite.org>
 *
 * A standalone tool to validate PATH environment variable for security issues.
 * Can be used independently or integrated with sudosh.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/**
 * Validate PATH for security issues
 */
int validate_path_security_standalone(const char *path_env, int verbose) {
    if (!path_env) {
        if (verbose) {
            printf("❌ CRITICAL: No PATH environment variable set\n");
            printf("   This is a serious security and functionality issue.\n");
        }
        return 0;
    }
    
    char *path_copy = safe_strdup(path_env);
    if (!path_copy) {
        if (verbose) printf("Error: Unable to analyze PATH\n");
        return 0;
    }
    
    char *dir, *saveptr;
    int issues_found = 0;
    int warnings_found = 0;
    int dir_count = 0;
    
    if (verbose) {
        printf("PATH Security Analysis\n");
        printf("======================\n\n");
        printf("Analyzing PATH: %s\n\n", path_env);
    }
    
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        dir_count++;
        
        if (verbose) {
            printf("%2d. %s", dir_count, dir);
        }
        
        /* Check for current directory (.) */
        if (strcmp(dir, ".") == 0) {
            if (verbose) {
                printf(" ❌ SECURITY ISSUE\n");
                printf("    Risk: Current directory (.) in PATH\n");
                printf("    Impact: Commands in current directory may be executed unintentionally\n");
                printf("    Fix: Remove '.' from PATH\n");
            }
            issues_found++;
        }
        /* Check for empty directory (::) */
        else if (strlen(dir) == 0) {
            if (verbose) {
                printf(" ❌ SECURITY ISSUE\n");
                printf("    Risk: Empty directory in PATH (equivalent to current directory)\n");
                printf("    Impact: Same as having '.' in PATH\n");
                printf("    Fix: Remove empty entries (::) from PATH\n");
            }
            issues_found++;
        }
        /* Check for relative paths */
        else if (dir[0] != '/') {
            if (verbose) {
                printf(" ❌ SECURITY ISSUE\n");
                printf("    Risk: Relative path '%s' in PATH\n", dir);
                printf("    Impact: Path resolution depends on current directory\n");
                printf("    Fix: Use absolute paths only\n");
            }
            issues_found++;
        }
        /* Check if directory exists and is accessible */
        else {
            struct stat st;
            if (stat(dir, &st) != 0) {
                if (verbose) {
                    printf(" ⚠️  WARNING\n");
                    printf("    Issue: Directory does not exist or is not accessible\n");
                    printf("    Error: %s\n", strerror(errno));
                }
                warnings_found++;
            } else if (!S_ISDIR(st.st_mode)) {
                if (verbose) {
                    printf(" ⚠️  WARNING\n");
                    printf("    Issue: PATH entry is not a directory\n");
                }
                warnings_found++;
            } else {
                if (verbose) {
                    printf(" ✅ OK\n");
                }
            }
        }
        
        dir = strtok_r(NULL, ":", &saveptr);
    }
    
    free(path_copy);
    
    if (verbose) {
        printf("\nSummary:\n");
        printf("--------\n");
        printf("Total directories: %d\n", dir_count);
        printf("Security issues: %d\n", issues_found);
        printf("Warnings: %d\n", warnings_found);
        printf("\n");
        
        if (issues_found == 0) {
            printf("✅ PATH is secure\n");
            printf("   • No current directory (.) in PATH\n");
            printf("   • No empty directories in PATH\n");
            printf("   • All paths are absolute\n");
        } else {
            printf("❌ Security issues found in PATH\n");
            printf("\nRecommendations:\n");
            printf("• Remove '.' (current directory) from PATH\n");
            printf("• Use absolute paths only\n");
            printf("• Remove empty entries (::)\n");
            printf("• Verify all directories exist and are intended\n");
            printf("\nSecure PATH example:\n");
            printf("export PATH=\"/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin\"\n");
        }
        
        if (warnings_found > 0) {
            printf("\n⚠️  %d warning(s) found - review PATH directories\n", warnings_found);
        }
    }
    
    return issues_found == 0;
}

/**
 * Clean PATH by removing dangerous entries
 */
char *clean_path(const char *path_env) {
    if (!path_env) return NULL;
    
    char *path_copy = safe_strdup(path_env);
    if (!path_copy) return NULL;
    
    char *clean_path_str = malloc(strlen(path_env) + 1);
    if (!clean_path_str) {
        free(path_copy);
        return NULL;
    }
    
    clean_path_str[0] = '\0';
    
    char *dir, *saveptr;
    int first = 1;
    
    dir = strtok_r(path_copy, ":", &saveptr);
    while (dir != NULL) {
        /* Skip dangerous entries */
        if (strcmp(dir, ".") != 0 &&           /* Skip current directory */
            strlen(dir) > 0 &&                 /* Skip empty entries */
            dir[0] == '/') {                   /* Only absolute paths */
            
            /* Check if directory exists */
            struct stat st;
            if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!first) {
                    size_t cap = strlen(path_env);
                    strncat(clean_path_str, ":", cap);
                }
                size_t remain = strlen(path_env) - strlen(clean_path_str) - 1;
                strncat(clean_path_str, dir, remain);
                first = 0;
            }
        }
        
        dir = strtok_r(NULL, ":", &saveptr);
    }
    
    free(path_copy);
    return clean_path_str;
}

/**
 * Print usage information
 */
void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("PATH Security Validation Tool\n\n");
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -q, --quiet    Quiet mode (exit code only)\n");
    printf("  -c, --clean    Show cleaned PATH\n");
    printf("  -f, --fix      Set cleaned PATH in environment\n");
    printf("  -p PATH        Validate specific PATH instead of environment\n");
    printf("\nExit codes:\n");
    printf("  0  PATH is secure\n");
    printf("  1  Security issues found\n");
    printf("  2  Error occurred\n");
    printf("\nExamples:\n");
    printf("  %s                    # Validate current PATH\n", program_name);
    printf("  %s -q                 # Quiet validation\n", program_name);
    printf("  %s -c                 # Show cleaned PATH\n", program_name);
    printf("  %s -p \"/bin:/usr/bin\" # Validate specific PATH\n", program_name);
}

/**
 * Main function for standalone tool
 */
int main(int argc, char *argv[]) {
    const char *path_to_check = NULL;
    int quiet = 0;
    int show_clean = 0;
    int fix_path = 0;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--clean") == 0) {
            show_clean = 1;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--fix") == 0) {
            fix_path = 1;
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                path_to_check = argv[++i];
            } else {
                fprintf(stderr, "Error: -p requires a PATH argument\n");
                return 2;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }
    
    /* Get PATH to check */
    if (!path_to_check) {
        path_to_check = getenv("PATH");
    }
    
    if (!path_to_check) {
        if (!quiet) {
            printf("❌ No PATH environment variable found\n");
        }
        return 1;
    }
    
    /* Show cleaned PATH if requested */
    if (show_clean) {
        char *cleaned = clean_path(path_to_check);
        if (cleaned) {
            printf("%s\n", cleaned);
            free(cleaned);
        } else {
            if (!quiet) printf("Error: Unable to clean PATH\n");
            return 2;
        }
        return 0;
    }
    
    /* Fix PATH if requested */
    if (fix_path) {
        char *cleaned = clean_path(path_to_check);
        if (cleaned) {
            if (setenv("PATH", cleaned, 1) == 0) {
                if (!quiet) printf("✅ PATH cleaned and set\n");
                if (!quiet) printf("New PATH: %s\n", cleaned);
            } else {
                if (!quiet) printf("❌ Failed to set cleaned PATH\n");
                free(cleaned);
                return 2;
            }
            free(cleaned);
        } else {
            if (!quiet) printf("Error: Unable to clean PATH\n");
            return 2;
        }
        return 0;
    }
    
    /* Validate PATH */
    int is_secure = validate_path_security_standalone(path_to_check, !quiet);
    
    return is_secure ? 0 : 1;
}
