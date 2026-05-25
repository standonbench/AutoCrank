// AutoCrank host selftest entry point. Wires up the stub PlaydateAPI then
// invokes selftest_run(). All PASS/FAIL lines go to stdout via the stubbed
// logToConsole — the final "SELFTEST: N passed, M failed" line is what CI
// would grep for.

#include "pd_api.h"
#include "game.h"

#include <stdio.h>

void host_setup_pd(void);

int main(void) {
    host_setup_pd();
    selftest_run();
    return 0;
}
