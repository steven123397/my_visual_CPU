#include "course_elf_loader.h"

/* 课程 ELF loader 实现：解析最小 RV64 ET_EXEC + PT_LOAD 子集，并产出 maps/stack 摘要。
   它不直接搬运段内容，只为 course_process_exec_image 提供可验证的进程映像元数据。 */

#define ELF64_EHDR_SIZE 64U
#define ELF64_PHDR_SIZE 56U
#define ELF_MAGIC0 0x7FU
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELFCLASS64 2U
#define ELFDATA2LSB 1U
#define EV_CURRENT 1U
#define ET_EXEC 2U
#define EM_RISCV 243U
#define PT_LOAD 1U
#define PF_X 1U
#define PF_W 2U
#define COURSE_ELF_DEFAULT_ENVP "PATH=/bin"

/* 按小端读取 2 字节。 */
static uint16_t read_le16(const uint8_t* image, size_t offset) {
    return (uint16_t)image[offset] |
           (uint16_t)((uint16_t)image[offset + 1U] << 8U);
}

/* 按小端读取 4 字节。 */
static uint32_t read_le32(const uint8_t* image, size_t offset) {
    return (uint32_t)image[offset] |
           ((uint32_t)image[offset + 1U] << 8U) |
           ((uint32_t)image[offset + 2U] << 16U) |
           ((uint32_t)image[offset + 3U] << 24U);
}

/* 按小端读取 8 字节。 */
static uint64_t read_le64(const uint8_t* image, size_t offset) {
    return (uint64_t)read_le32(image, offset) |
           ((uint64_t)read_le32(image, offset + 4U) << 32U);
}

/* 把 load 结果清零，用于出错回滚或初始化。 */
static void clear_load(course_elf_load_result_t* load) {
    size_t i = 0;
    size_t j = 0;

    if (load == 0) {
        return;
    }
    load->entry_pc = 0;
    load->user_sp = 0;
    load->map_count = 0;
    load->argc = 0;
    for (i = 0; i < COURSE_ELF_ARGV_MAX; ++i) {
        load->argv[i] = '\0';
    }
    for (i = 0; i < COURSE_ELF_ENVP_MAX; ++i) {
        load->envp[i] = '\0';
    }
    for (i = 0; i < COURSE_ELF_MAX_MAPS; ++i) {
        load->maps[i].start = 0;
        load->maps[i].end = 0;
        load->maps[i].flags = 0;
        load->maps[i].cow = false;
        for (j = 0; j < COURSE_ELF_MAP_NAME_MAX; ++j) {
            load->maps[i].name[j] = '\0';
        }
    }
}

/* 安全拷贝字符串到定长缓冲并补 NUL。 */
static void copy_str(char* out, size_t out_size, const char* value) {
    size_t i = 0;

    if (out == 0 || out_size == 0) {
        return;
    }
    if (value != 0) {
        while (value[i] != '\0' && i + 1U < out_size) {
            out[i] = value[i];
            i += 1U;
        }
    }
    out[i] = '\0';
}

/* 向 load 结果追加一段映射摘要，满了或区间非法则失败。 */
static bool add_map(course_elf_load_result_t* load,
                    const char* name,
                    uintptr_t start,
                    uintptr_t end,
                    uint32_t flags,
                    bool cow) {
    course_elf_map_t* map = 0;

    if (load == 0 || name == 0 || start >= end ||
        load->map_count >= COURSE_ELF_MAX_MAPS) {
        return false;
    }
    map = &load->maps[load->map_count];
    copy_str(map->name, sizeof(map->name), name);
    map->start = start;
    map->end = end;
    map->flags = flags;
    map->cow = cow;
    load->map_count += 1U;
    return true;
}

course_elf_result_t course_elf_loader_load(const uint8_t* image,
                                           size_t image_size,
                                           const char* argv,
                                           course_elf_load_result_t* out_load) {
    const uint64_t phoff_offset = 32U;
    const uint64_t entry_offset = 24U;
    const uint64_t ehsize_offset = 52U;
    const uint64_t phentsize_offset = 54U;
    const uint64_t phnum_offset = 56U;
    uint64_t entry = 0;
    uint64_t phoff = 0;
    uint16_t ehsize = 0;
    uint16_t phentsize = 0;
    uint16_t phnum = 0;
    uintptr_t heap_start = 0;
    bool saw_load = false;
    bool entry_in_executable_load = false;
    uint16_t i = 0;

    clear_load(out_load);
    if (image == 0 || out_load == 0 || image_size < 4U) {
        return COURSE_ELF_ERR_BAD_HEADER;
    }
    if (image[0] != ELF_MAGIC0 || image[1] != ELF_MAGIC1 ||
        image[2] != ELF_MAGIC2 || image[3] != ELF_MAGIC3) {
        return COURSE_ELF_ERR_BAD_MAGIC;
    }
    if (image_size < ELF64_EHDR_SIZE) {
        return COURSE_ELF_ERR_BAD_HEADER;
    }
    if (image[4] != ELFCLASS64 || image[5] != ELFDATA2LSB ||
        image[6] != EV_CURRENT ||
        read_le16(image, 16U) != ET_EXEC ||
        read_le16(image, 18U) != EM_RISCV ||
        read_le32(image, 20U) != EV_CURRENT) {
        return COURSE_ELF_ERR_UNSUPPORTED;
    }

    /* 从这里开始所有 offset/数量都必须先通过边界检查，避免坏 ELF 读越界。 */
    entry = read_le64(image, (size_t)entry_offset);
    phoff = read_le64(image, (size_t)phoff_offset);
    ehsize = read_le16(image, (size_t)ehsize_offset);
    phentsize = read_le16(image, (size_t)phentsize_offset);
    phnum = read_le16(image, (size_t)phnum_offset);
    if (ehsize != ELF64_EHDR_SIZE || phentsize != ELF64_PHDR_SIZE ||
        phnum == 0U || phoff > image_size ||
        (uint64_t)phnum > ((uint64_t)image_size - phoff) / phentsize) {
        return COURSE_ELF_ERR_BAD_HEADER;
    }

    out_load->entry_pc = (uintptr_t)entry;
    copy_str(out_load->argv, sizeof(out_load->argv), argv);
    copy_str(out_load->envp, sizeof(out_load->envp), COURSE_ELF_DEFAULT_ENVP);
    out_load->argc = out_load->argv[0] != '\0' ? 1U : 0U;

    for (i = 0; i < phnum; ++i) {
        const size_t offset = (size_t)phoff + ((size_t)i * phentsize);
        const uint32_t p_type = read_le32(image, offset);
        const uint32_t p_flags = read_le32(image, offset + 4U);
        const uint64_t p_offset = read_le64(image, offset + 8U);
        const uint64_t p_vaddr = read_le64(image, offset + 16U);
        const uint64_t p_filesz = read_le64(image, offset + 32U);
        const uint64_t p_memsz = read_le64(image, offset + 40U);
        const char* name = (p_flags & PF_X) != 0U ? "code" : "data";
        uintptr_t end = 0;

        if (p_type != PT_LOAD) {
            continue;
        }
        /* filesz 必须落在镜像内，memsz 可大于 filesz，用于表示 BSS。 */
        if (p_memsz < p_filesz || p_offset > image_size ||
            p_filesz > (uint64_t)image_size - p_offset ||
            p_vaddr == 0U || p_memsz == 0U ||
            p_vaddr + p_memsz < p_vaddr) {
            clear_load(out_load);
            return COURSE_ELF_ERR_BAD_PROGRAM_HEADER;
        }

        saw_load = true;
        if ((p_flags & PF_X) != 0U &&
            entry >= p_vaddr &&
            entry < p_vaddr + p_memsz) {
            entry_in_executable_load = true;
        }
        end = (uintptr_t)(p_vaddr + p_memsz);
        if (!add_map(out_load,
                     name,
                     (uintptr_t)p_vaddr,
                     end,
                     p_flags,
                     false)) {
            clear_load(out_load);
            return COURSE_ELF_ERR_BAD_PROGRAM_HEADER;
        }
        if (end > heap_start) {
            heap_start = end;
        }
    }

    if (!saw_load) {
        clear_load(out_load);
        return COURSE_ELF_ERR_NO_LOAD;
    }
    if (!entry_in_executable_load) {
        clear_load(out_load);
        return COURSE_ELF_ERR_BAD_PROGRAM_HEADER;
    }
    if (heap_start == 0U) {
        heap_start = (uintptr_t)entry;
    }
    heap_start = (heap_start + 0xFFFU) & ~(uintptr_t)0xFFFU;
    /* loader 固定补 heap/stack 两段，让 procfs maps 和用户程序 smoke 都有完整布局。 */
    if (!add_map(out_load,
                 "heap",
                 heap_start,
                 heap_start + COURSE_ELF_USER_HEAP_SIZE,
                 PF_W,
                 false) ||
        !add_map(out_load,
                 "stack",
                 (uintptr_t)(COURSE_ELF_USER_STACK_TOP -
                             COURSE_ELF_USER_STACK_SIZE),
                 (uintptr_t)COURSE_ELF_USER_STACK_TOP,
                 PF_W,
                 false)) {
        clear_load(out_load);
        return COURSE_ELF_ERR_BAD_PROGRAM_HEADER;
    }
    out_load->user_sp = (uintptr_t)COURSE_ELF_USER_STACK_TOP;
    return COURSE_ELF_OK;
}
