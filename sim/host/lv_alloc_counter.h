// sim/host/lv_alloc_counter.h —— LVGL 自定义分配器的计数接口。
//
// 为什么不用 CLIB + 宿主 malloc 统计？
//   实测发现 macOS 的 malloc_zone_statistics 在这台机器上不可靠
//   （max_size_in_use 读出来是 0，display 阶段报 +70KB，与内置池模式的
//    6.1KB 直接矛盾）。用一个要"加注解道歉"的数字没有意义。
//
// 所以模拟器默认用 LV_STDLIB_CUSTOM：
//   语义与真机的 CONFIG_LV_USE_CLIB_MALLOC=y 完全一致（都是直接走 malloc），
//   但每一次申请/释放都经过我们自己的包装，可以精确统计 LVGL 真实占用。
#pragma once

#include <stddef.h>

// cur_bytes  : 当前 LVGL 存活的字节数（按 malloc 实际可用的粒度计）
// peak_bytes : 历史峰值
// live_blocks: 当前存活块数
// total_allocs: 累计申请次数
void sim_lv_alloc_stats(size_t *cur_bytes, size_t *peak_bytes,
                        size_t *live_blocks, size_t *total_allocs);
