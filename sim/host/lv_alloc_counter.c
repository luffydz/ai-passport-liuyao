// sim/host/lv_alloc_counter.c —— LV_STDLIB_CUSTOM 的实现。
//
// 行为与 CLIB 等价（直接 malloc/free/realloc），额外做了精确计数。
// 块大小用 macOS 的 malloc_size() 取实际可用字节数 —— 它包含了分配器的
// rounding 开销，所以统计结果偏保守（偏大），不会低估。
#include "lv_alloc_counter.h"
#include "lvgl.h"

#include <malloc/malloc.h>   // malloc_size()
#include <stdlib.h>

static size_t s_cur;          // 当前存活字节
static size_t s_peak;         // 峰值
static size_t s_allocs;       // 累计申请次数
static size_t s_frees;        // 累计释放次数

void sim_lv_alloc_stats(size_t *cur_bytes, size_t *peak_bytes,
                        size_t *live_blocks, size_t *total_allocs)
{
    if (cur_bytes)    *cur_bytes = s_cur;
    if (peak_bytes)   *peak_bytes = s_peak;
    if (live_blocks)  *live_blocks = s_allocs - s_frees;
    if (total_allocs) *total_allocs = s_allocs;
}

// ---------------------------------------------------------------------------
// LVGL 要求的自定义分配器接口（LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM）
//
// 函数清单照抄 src/stdlib/clib/lv_mem_core_clib.c —— CLIB 是我们的语义基准，
// 少一个都会在链接期报 undefined（lv_mem_init / lv_mem_deinit 就是这么发现的）。
// ---------------------------------------------------------------------------
void lv_mem_init(void) { /* 无需初始化 */ }

void lv_mem_deinit(void) { /* 无需反初始化 */ }

// 内存池概念只对内置分配器有意义，CUSTOM 下不支持
lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool)
{
    LV_UNUSED(pool);
}

void *lv_malloc_core(size_t size)
{
    void *p = malloc(size);
    if (p) {
        const size_t bytes = malloc_size(p);
        s_cur += bytes;
        if (s_cur > s_peak) s_peak = s_cur;
        s_allocs++;
    }
    return p;
}

void lv_free_core(void *p)
{
    if (!p) return;
    const size_t bytes = malloc_size(p);
    s_cur -= (bytes <= s_cur) ? bytes : s_cur;
    s_frees++;
    free(p);
}

void *lv_realloc_core(void *p, size_t new_size)
{
    if (!p) return lv_malloc_core(new_size);

    const size_t old_bytes = malloc_size(p);
    void *np = realloc(p, new_size);
    if (!np) return NULL;   // 失败时原块仍在，计数不动

    const size_t new_bytes = malloc_size(np);
    s_cur = s_cur - old_bytes + new_bytes;
    if (s_cur > s_peak) s_peak = s_cur;
    return np;
}

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    if (!mon_p) return;
    // 没有固定池，所以 total 用「峰值」表达，free = 峰值 - 当前
    mon_p->total_size = s_peak;
    mon_p->free_size = (s_peak > s_cur) ? (s_peak - s_cur) : 0;
    mon_p->free_biggest_size = mon_p->free_size;
    mon_p->used_cnt = s_allocs - s_frees;
    mon_p->free_cnt = 0;
    mon_p->max_used = s_peak;
    mon_p->used_pct = s_peak ? (uint8_t)(s_cur * 100 / s_peak) : 0;
    mon_p->frag_pct = 0;
}

lv_result_t lv_mem_test_core(void)
{
    return LV_RESULT_OK;
}
