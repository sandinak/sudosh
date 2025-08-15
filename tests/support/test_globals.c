#include "sudosh.h"

// Provide global variables required by library objects when linking unit/integration tests
struct ansible_detection_info *global_ansible_info = NULL;
struct ai_detection_info *global_ai_info = NULL;

int test_mode = 1;

/* Shell enhancements default for tests */
int rc_alias_import_enabled = 1;

