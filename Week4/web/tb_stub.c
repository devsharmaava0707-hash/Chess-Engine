/*
 * Syzygy tablebase stub for the WebAssembly build.
 *
 * Tablebase files can't be memory-mapped from disk in a browser, so the web
 * build links this stub instead of tablebase/tbprobe.c. It makes the engine
 * behave exactly as it does when no tablebases are found (TB_LARGEST == 0),
 * which is what your native build prints today ("TB_LARGEST = 0").
 */
#include <tbprobe.h>

unsigned TB_LARGEST = 0;

bool tb_init_impl(const char *path)
{
    (void)path;
    TB_LARGEST = 0;
    return false;
}

bool tb_init(const char *path)
{
    return tb_init_impl(path);
}

void tb_free(void) {}

unsigned tb_probe_wdl_impl(uint64_t white, uint64_t black, uint64_t kings,
                           uint64_t queens, uint64_t rooks, uint64_t bishops,
                           uint64_t knights, uint64_t pawns, unsigned ep,
                           bool turn)
{
    (void)white; (void)black; (void)kings; (void)queens; (void)rooks;
    (void)bishops; (void)knights; (void)pawns; (void)ep; (void)turn;
    return TB_RESULT_FAILED;
}

unsigned tb_probe_root_impl(uint64_t white, uint64_t black, uint64_t kings,
                            uint64_t queens, uint64_t rooks, uint64_t bishops,
                            uint64_t knights, uint64_t pawns, unsigned rule50,
                            unsigned ep, bool turn, unsigned *results)
{
    (void)white; (void)black; (void)kings; (void)queens; (void)rooks;
    (void)bishops; (void)knights; (void)pawns; (void)rule50; (void)ep;
    (void)turn; (void)results;
    return TB_RESULT_FAILED;
}
