/*
 * LVGL's allocator (LV_STDLIB_CUSTOM in lv_conf.h): PSRAM first, internal
 * RAM only when PSRAM is full. Plain malloc() keeps everything under
 * CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL (4 KB) internal, and the whole widget
 * tree is made of such blocks: it used up the internal RAM that DMA, Wi-Fi
 * and FreeRTOS need, until a settings save couldn't even get a file lock.
 */
#include "esp_heap_caps.h"
#include "lvgl.h"

#define PSRAM_FIRST 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes) { return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) {}

void *lv_malloc_core(size_t size) { return heap_caps_malloc_prefer(size, PSRAM_FIRST); }
void *lv_realloc_core(void *p, size_t new_size) { return heap_caps_realloc_prefer(p, new_size, PSRAM_FIRST); }
void lv_free_core(void *p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t *mon_p)
{
    mon_p->total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    mon_p->free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    mon_p->free_biggest_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
