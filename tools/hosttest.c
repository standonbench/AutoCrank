// ---------------------------------------------------------------------------
// Headless host test harness.
//
// Runs the mechanics self-test suite (selftest_run) natively from the command
// line, with NO Playdate Simulator. The whole game compiles on host already
// (that's how the Simulator .dylib is built); we just swap main.c's real
// eventHandler/pd for a minimal stub and call selftest_run() directly.
//
// The self-test path only ever touches three Playdate calls:
//   • system->logToConsole — DLOG, incl. each "PASS/FAIL <name>" + the summary
//   • system->error        — ASSERT (shouldn't fire in passing tests)
//   • file->open           — save_write(); returning NULL makes it no-op safely
// Everything else stays NULL: the sfx_* helpers guard on NULL synths, and the
// tests never render or read input. See the build recipe in tools/runtests.sh.
//
// Build + run:  sh tools/runtests.sh
// ---------------------------------------------------------------------------
#include "game.h"
#include <stdio.h>
#include <stdarg.h>

static void stub_log(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); putchar('\n');
}

static SDFile* stub_open(const char* name, FileOptions mode) {
    (void)name; (void)mode;
    return NULL;   // save_write()/save_load() both handle a NULL file gracefully
}

static struct playdate_sys  g_sys;
static struct playdate_file g_file;
static PlaydateAPI          g_api;

// Satisfies `extern PlaydateAPI* pd;` for every translation unit (normally
// defined in main.c, which this harness replaces).
PlaydateAPI* pd = NULL;

int main(void) {
    g_sys.logToConsole = stub_log;
    g_sys.error        = stub_log;
    g_file.open        = stub_open;
    g_api.system = &g_sys;
    g_api.file   = &g_file;
    pd = &g_api;

    selftest_run();
    return 0;
}
