#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Sv39 虚拟内存子系统：地址空间、用户进程、内存对象与缺页处理。
   提供 Sv39 三级页表、内核/用户区映射、写时复制对象与 fault-driven 映射的统一接口。 */
#define VM_PAGE_READ (1ULL << 1)
#define VM_PAGE_WRITE (1ULL << 2)
#define VM_PAGE_EXEC (1ULL << 3)
#define VM_PAGE_USER (1ULL << 4)
#define VM_PROCESS_MAX_USER_REGIONS 48U
#define VM_OBJECT_ANON_SLOT_TABLE_COUNT 8U

typedef struct VmAddressSpace vm_address_space_t;

typedef enum VmRegionObjectMode {
    VM_REGION_OBJECT_NONE = 0,
    VM_REGION_OBJECT_MAPPED,
    VM_REGION_OBJECT_FAULT,
} vm_region_object_mode_t;

typedef enum VmObjectBackingKind {
    VM_OBJECT_BACKING_NONE = 0,
    VM_OBJECT_BACKING_PHYSICAL,
    VM_OBJECT_BACKING_ANON,
} vm_object_backing_kind_t;

typedef struct VmObjectPhysicalBacking {
    uintptr_t base_paddr;
} vm_object_physical_backing_t;

typedef struct VmObjectAnonBacking {
    uintptr_t* page_slots;
    size_t page_count;
    uintptr_t* extra_page_slots[VM_OBJECT_ANON_SLOT_TABLE_COUNT - 1U];
} vm_object_anon_backing_t;

typedef struct VmObject {
    bool initialized;
    vm_object_backing_kind_t backing_kind;
    size_t size;
    size_t attachment_count;
    union {
        vm_object_physical_backing_t physical;
        vm_object_anon_backing_t anon;
    } backing;
} vm_object_t;

typedef struct VmUserRegion {
    vm_address_space_t* address_space;
    uintptr_t vaddr;
    size_t size;
    uint64_t flags;
    bool registered;
    vm_object_t* object;
    size_t object_offset;
    vm_region_object_mode_t object_mode;
} vm_user_region_t;

typedef struct VmProcess {
    vm_address_space_t* address_space;
    uintptr_t entry_pc;
    uintptr_t user_sp;
    vm_user_region_t* user_regions[VM_PROCESS_MAX_USER_REGIONS];
} vm_process_t;

typedef struct VmProcessUserRegionBinding {
    vm_user_region_t* region;
    uintptr_t vaddr;
    size_t size;
    uint64_t flags;
    vm_object_t* object;
    size_t object_offset;
    vm_region_object_mode_t object_mode;
} vm_process_user_region_binding_t;

/* 从地址空间池分配一个地址空间并建好根页表，输出到 out_space。 */
bool vm_address_space_create(vm_address_space_t** out_space);
/* 把地址空间登记为当前活跃（不写 satp）。 */
bool vm_address_space_activate(vm_address_space_t* address_space);
/* 地址空间是否为当前活跃者。 */
bool vm_address_space_is_active(const vm_address_space_t* address_space);
/* 地址空间是否已写 satp 开启分页。 */
bool vm_address_space_is_enabled(const vm_address_space_t* address_space);
/* 关闭分页并清除活跃登记，回收 satp。 */
bool vm_address_space_disable(vm_address_space_t* address_space);
/* 仍绑定用户区时拒绝；否则递归释放页表页并归还地址空间池。 */
bool vm_address_space_destroy(vm_address_space_t* address_space);
/* 用 1GB superpage 做 identity 映射（kernel 用）。 */
bool vm_address_space_map_identity_1g(vm_address_space_t* address_space,
                                      uintptr_t base,
                                      uint64_t flags);
/* 把内核/MMIO 区间按 4KB 页映射到地址空间。 */
bool vm_address_space_map_kernel_range(vm_address_space_t* address_space,
                                       uintptr_t vaddr,
                                       uintptr_t paddr,
                                       size_t size,
                                       uint64_t flags);
/* 初始化并注册一个用户区（vaddr/size/flags），登记到地址空间。 */
bool vm_address_space_user_region_init(vm_address_space_t* address_space,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags);
/* 登记一段内核 fault range：缺页时按其 paddr/flags 映射。 */
bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags);
/* 登记缺页动作：命中时跳过触发指令。 */
bool vm_address_space_register_fault_skip(vm_address_space_t* address_space,
                                          uint64_t cause,
                                          uintptr_t vaddr,
                                          size_t size);
/* 登记缺页动作：命中时把 sepc 设为 slot 指向的地址。 */
bool vm_address_space_register_fault_resume_slot(
    vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t vaddr,
    size_t size,
    volatile uintptr_t* resume_pc_slot);
/* 写 satp 开启 Sv39 分页并刷新 TLB，置 enabled。 */
bool vm_address_space_enable(vm_address_space_t* address_space);
/* 返回根页表物理地址。 */
uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space);
/* 返回组装好的 satp 值。 */
uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space);
/* 把进程绑定到地址空间，要求进程此前为干净状态。 */
bool vm_process_create(vm_process_t* process, vm_address_space_t* address_space);
/* 校验可运行后开启地址空间并登记为当前活跃进程。 */
bool vm_process_activate(vm_process_t* process);
/* 进程是否为当前活跃进程。 */
bool vm_process_is_active(const vm_process_t* process);
/* 摘除进程的一个用户区并清理其对象绑定与映射。 */
bool vm_process_remove_user_region(vm_process_t* process,
                                   vm_user_region_t* region);
/* 摘除所有用户区并清空进程上下文，回到未绑定状态。 */
bool vm_process_reset(vm_process_t* process);
/* 给进程分配槽位并注册一个用户区。 */
bool vm_process_user_region_init(vm_process_t* process,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags);
/* 按一组 binding 批量绑定用户区，失败整体回滚。 */
bool vm_process_bind_user_regions(
    vm_process_t* process,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count);
/* 立即把对象映射到进程用户区（指定对象偏移）。 */
bool vm_process_map_object_region_at(vm_process_t* process,
                                     vm_user_region_t* region,
                                     uintptr_t vaddr,
                                     size_t size,
                                     uint64_t flags,
                                     vm_object_t* object,
                                     size_t object_offset);
/* 同上，对象偏移为 0。 */
bool vm_process_map_object_region(vm_process_t* process,
                                  vm_user_region_t* region,
                                  uintptr_t vaddr,
                                  size_t size,
                                  uint64_t flags,
                                  vm_object_t* object);
/* 把对象设为进程用户区的 fault 对象（缺页时按需映射）。 */
bool vm_process_set_fault_object_region_at(vm_process_t* process,
                                           vm_user_region_t* region,
                                           uintptr_t vaddr,
                                           size_t size,
                                           uint64_t flags,
                                           vm_object_t* object,
                                           size_t object_offset);
/* 同上，对象偏移为 0。 */
bool vm_process_set_fault_object_region(vm_process_t* process,
                                        vm_user_region_t* region,
                                        uintptr_t vaddr,
                                        size_t size,
                                        uint64_t flags,
                                        vm_object_t* object);
/* 设置进程入口 PC 与用户栈顶，校验落在可执行/可写区。 */
bool vm_process_set_user_context(vm_process_t* process,
                                 uintptr_t entry_pc,
                                 uintptr_t user_sp);
/* 进程是否具备可运行的用户上下文。 */
bool vm_process_is_runnable(const vm_process_t* process);

/* 复位对象：匿名对象释放所有页与 slot 表，物理对象直接清描述符。 */
bool vm_object_reset(vm_object_t* object);
/* 初始化为物理后端对象（base paddr + size）。 */
bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size);
/* 初始化为匿名对象，分配 slot 表，页按需分配。 */
bool vm_object_init_anon(vm_object_t* object, size_t size);
/* 解析对象某页的物理地址，匿名对象未分配时立即分配。 */
bool vm_object_resolve_page_for_write(vm_object_t* object,
                                      size_t page_offset,
                                      uintptr_t* out_paddr);
/* 解除用户区与对象的绑定并清掉对应页映射。 */
bool vm_user_region_clear_object(vm_user_region_t* region);
/* 立即把对象映射进用户区（指定偏移）。 */
bool vm_user_region_map_object_at(vm_user_region_t* region,
                                  vm_object_t* object,
                                  size_t object_offset);
/* 同上，偏移为 0。 */
bool vm_user_region_map_object(vm_user_region_t* region, vm_object_t* object);
/* 把对象设为用户区的 fault 对象（缺页驱动映射）。 */
bool vm_user_region_set_fault_object_at(vm_user_region_t* region,
                                        vm_object_t* object,
                                        size_t object_offset);
/* 同上，偏移为 0。 */
bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object);
/* 解除用户区内某页的映射并刷新 TLB。 */
bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr);
/* vaddr..vaddr+size 是否落在该用户区内。 */
bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size);
/* 区间是否完全落在内核虚拟地址窗口。 */
bool vm_range_is_kernel(uintptr_t vaddr, size_t size);
/* 区间是否完全落在用户虚拟地址窗口。 */
bool vm_range_is_user(uintptr_t vaddr, size_t size);
/* 内核虚拟地址窗口起始。 */
uintptr_t vm_kernel_base(void);
/* 内核虚拟地址窗口结束。 */
uintptr_t vm_kernel_limit(void);
/* 用户虚拟地址窗口起始。 */
uintptr_t vm_user_base(void);
/* 用户虚拟地址窗口结束。 */
uintptr_t vm_user_limit(void);
/* 处理一次缺页：用户区对象按需映射、内核 fault range 映射或执行注册动作。 */
bool vm_handle_page_fault(vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval);
/* 执行 sfence.vma 刷新 TLB。 */
void vm_flush_tlb(void);
