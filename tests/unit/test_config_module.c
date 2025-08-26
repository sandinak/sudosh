#include "../test_framework.h"
#include "../../src/sudosh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

/* Minimal tests to exercise config.c code paths */

static const char *config_fixture =
    "auth_cache_timeout=120\n"
    "inactivity_timeout=600\n"
    "max_command_length=8192\n"
    "verbose_mode=true\n"
    "test_mode=1\n"
    "log_facility=auth\n"
    "cache_directory=/tmp/sudosh-cache\n"
    "lock_directory=/tmp/sudosh-locks\n"
    "ansible_detection_enabled=0\n"
    "ansible_detection_force=true\n"
    "rc_alias_import_enabled=false\n"
    "ansible_detection_verbose=1\n"
    "ansible_detection_confidence_threshold=85\n";

static int test_config_parse_and_validate() {
    sudosh_config_t *cfg = sudosh_config_init();
    TEST_ASSERT_NOT_NULL(cfg, "config init should succeed");

    char *tmp = create_temp_file(config_fixture);
    TEST_ASSERT_NOT_NULL(tmp, "temp config file created");

    sudosh_error_t rc = sudosh_config_load(cfg, tmp);
    TEST_ASSERT_EQ(SUDOSH_SUCCESS, rc, "config load success");

    rc = sudosh_config_validate(cfg);
    TEST_ASSERT_EQ(SUDOSH_SUCCESS, rc, "config validate success");

    /* Spot check a few fields */
    TEST_ASSERT_EQ(120, cfg->auth_cache_timeout, "auth_cache_timeout parsed");
    TEST_ASSERT_EQ(600, cfg->inactivity_timeout, "inactivity_timeout parsed");
    TEST_ASSERT_EQ(8192, cfg->max_command_length, "max_command_length parsed");
    TEST_ASSERT(cfg->verbose_mode == 1, "verbose_mode parsed");
    TEST_ASSERT(cfg->test_mode == 1, "test_mode parsed");
    TEST_ASSERT_STR_EQ("auth", cfg->log_facility, "log_facility parsed");
    TEST_ASSERT_STR_EQ("/tmp/sudosh-cache", cfg->cache_directory, "cache_directory parsed");
    TEST_ASSERT_STR_EQ("/tmp/sudosh-locks", cfg->lock_directory, "lock_directory parsed");
    TEST_ASSERT(cfg->ansible_detection_enabled == 0, "ansible_detection_enabled parsed");
    TEST_ASSERT(cfg->ansible_detection_force == 1, "ansible_detection_force parsed");
    TEST_ASSERT(cfg->rc_alias_import_enabled == 0, "rc_alias_import_enabled parsed");
    TEST_ASSERT(cfg->ansible_detection_verbose == 1, "ansible_detection_verbose parsed");
    TEST_ASSERT_EQ(85, cfg->ansible_detection_confidence_threshold, "confidence threshold parsed");

    sudosh_config_free(cfg);
    remove_temp_file(tmp);
    return 1;
}

static int test_config_invalid_threshold_logs_warning() {
    sudosh_config_t *cfg = sudosh_config_init();
    TEST_ASSERT_NOT_NULL(cfg, "config init");

    const char *bad_conf = "ansible_detection_confidence_threshold=1234\n";
    char *tmp = create_temp_file(bad_conf);
    TEST_ASSERT_NOT_NULL(tmp, "temp config");

    sudosh_error_t rc = sudosh_config_load(cfg, tmp);
    TEST_ASSERT_EQ(SUDOSH_SUCCESS, rc, "load returns success despite warning");

    /* Value should remain default (70) after invalid input */
    TEST_ASSERT_EQ(70, cfg->ansible_detection_confidence_threshold, "invalid threshold ignored");

    sudosh_config_free(cfg);
    remove_temp_file(tmp);
    return 1;
}

TEST_SUITE_BEGIN("Config Module Unit Tests")
    RUN_TEST(test_config_parse_and_validate);
    RUN_TEST(test_config_invalid_threshold_logs_warning);
TEST_SUITE_END()

