#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void pmm_init(void);
void* pmm_alloc_page(void);
bool pmm_free_page(void* page);

uintptr_t pmm_managed_start(void);
uintptr_t pmm_managed_end(void);
size_t pmm_total_pages(void);
size_t pmm_free_pages(void);
size_t pmm_used_pages(void);
