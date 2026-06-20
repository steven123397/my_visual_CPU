#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 物理页管理器（PMM）：bitmap 管理的页框分配器，接管早期分配器之后的物理内存。 */
void pmm_init(void);
/* 分配一个 4KB 物理页，返回地址；无空闲返回 NULL。 */
void* pmm_alloc_page(void);
/* 释放一个已分配且对齐的物理页。 */
bool pmm_free_page(void* page);

/* PMM 管理的物理内存起始地址。 */
uintptr_t pmm_managed_start(void);
/* PMM 管理的物理内存结束地址。 */
uintptr_t pmm_managed_end(void);
/* PMM 管理的总页数。 */
size_t pmm_total_pages(void);
/* 当前空闲页数。 */
size_t pmm_free_pages(void);
/* 当前已用页数。 */
size_t pmm_used_pages(void);
