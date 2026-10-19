#include "bk_test.h"

int bk_test_failures;
int bk_test_checks;
const char *bk_test_current = "";

int bk_test_finish(const char *suite)
{
    printf("%s: %d checks, %d failures\n", suite, bk_test_checks, bk_test_failures);
    return bk_test_failures == 0 ? 0 : 1;
}
