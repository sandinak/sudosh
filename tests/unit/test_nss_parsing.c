#include "../test_framework.h"
#include "../../src/sudosh.h"

/* Global test counters */
int test_count = 0;
int test_passes = 0;
int test_failures = 0;

static const char *nss_fixture =
    "passwd: files sssd\n"
    "sudoers: files\n";

static int test_read_nss_config_basic() {
    /* Create a temp file and point NSS_CONF_PATH via a weak hook if supported.
       Since code reads fixed paths, we at least exercise parse_nss_line indirectly
       by calling read_nss_config() on the real system (best-effort). */
    struct nss_config *cfg = read_nss_config();
    TEST_ASSERT_NOT_NULL(cfg, "read_nss_config should return config");

    /* Ensure at least defaults are present */
    TEST_ASSERT_NOT_NULL(cfg->passwd_sources, "passwd sources present");
    TEST_ASSERT_NOT_NULL(cfg->sudoers_sources, "sudoers sources present");

    free_nss_config(cfg);
    return 1;
}

TEST_SUITE_BEGIN("NSS Parsing Unit Tests")
    RUN_TEST(test_read_nss_config_basic);
TEST_SUITE_END()

