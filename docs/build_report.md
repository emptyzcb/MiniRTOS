# MiniOS - 轻量级嵌入式实时操作系统

## 项目概述

MiniOS 是一个面向 ARM Cortex-M3 (STM32F103) 的轻量级嵌入式实时操作系统，设计参考 FreeRTOS 核心架构。用于学习 RTOS 内核原理和嵌入式系统开发。

- **目标平台**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB RAM)
- **语言**: C11 + ARM Assembly
- **构建工具**: arm-none-eabi-gcc + Make

---

## 项目结构

```
D:\A_stm32_project\
├── config\
│   └── os_config.h           # 内核配置宏 (任务数、堆大小、tick频率等)
├── common\
│   └── os_types.h            # 通用类型定义 (状态码、任务句柄等)
├── include\
│   └── os.h                  # 对外统一头文件 (应用层只需包含此文件)
├── kernel\
│   ├── heap4.h               # Heap-4 内存管理头文件
│   ├── heap4.c               # Heap-4 实现 (首次适配+合并空闲块)
│   ├── task.h                # 任务管理头文件
│   ├── task.c                # 任务管理实现 (TCB、就绪链表、tick处理)
│   ├── scheduler.h           # 调度器头文件
│   ├── scheduler.c           # 调度器实现 (优先级抢占+时间片轮转)
│   ├── kernel.h              # 内核核心头文件
│   └── kernel.c              # 内核初始化、系统tick管理
├── port\
│   ├── port.h                # 硬件抽象层头文件
│   ├── port.c                # Cortex-M3 移植层 (PendSV上下文切换、SysTick)
│   ├── startup_stm32f103.s   # 启动文件 (向量表、.data/.bss初始化)
│   └── stm32f103.ld          # 链接脚本 (Flash/RAM布局)
├── app\
│   └── main.c                # 应用入口 (演示3个不同优先级的任务)
├── Makefile                  # 构建系统
├── .gitignore
└── docs\
    └── build_report.md       # 本文档
```

---

## 核心模块详解

### 1. Heap-4 内存管理 (`kernel/heap4.c`)

**算法**: 首次适配 (First-Fit) + 相邻空闲块合并 (Coalescing)

**内存布局**:
```
[Block Header][Data Area][Block Header][Data Area]...
```

每个 Block Header 包含:
- `size`: 数据区大小 (最高位用作分配标记)
- `next`: 指向下一个块的指针
- `allocated`: 分配状态

**关键特性**:
- 分配时从空闲链表中找到第一个足够大的块，必要时分割
- 释放时标记为空闲，自动与前后相邻空闲块合并
- 空闲链表按地址排序，保证合并的正确性
- O(n) 分配，O(1) 释放（摊销）

**API**:
```c
void  os_heap_init(void);
void* os_heap_alloc(uint32_t size);
void* os_heap_calloc(uint32_t count, uint32_t size);
void  os_heap_free(void *ptr);
uint32_t os_heap_get_free_size(void);
uint32_t os_heap_get_largest_free_block(void);
uint32_t os_heap_get_min_free_size(void);
```

### 2. 任务管理 (`kernel/task.c`)

**任务控制块 (TCB)**:
```c
typedef struct os_tcb {
    os_stack_t      *stack_ptr;       // 当前栈指针 (必须在首位)
    os_stack_t      *stack_base;      // 栈底地址
    uint32_t        stack_size;       // 栈大小
    os_prio_t       priority;         // 优先级
    os_task_state_t state;            // 任务状态
    os_tick_t       delay_ticks;      // 阻塞剩余tick数
    char            name[16];         // 任务名
    os_task_func_t  entry_func;       // 入口函数
    void            *param;           // 入口参数
    struct os_tcb   *next, *prev;     // 链表指针
    uint32_t        stack_high_water; // 栈使用峰值
} os_tcb_t;
```

**就绪链表**: 每个优先级维护一个双向链表，O(1) 插入/删除。

**任务状态机**:
```
Created --> READY --> RUNNING --> READY
                   \-> SUSPENDED
                   \-> BLOCKED (delay)
```

**API**:
```c
os_status_t os_task_create(func, name, param, priority, stack, size, handle);
os_status_t os_task_delete(handle);
os_status_t os_task_suspend(handle);
os_status_t os_task_resume(handle);
os_status_t os_task_delay(ticks);
```

### 3. 调度器 (`kernel/scheduler.c`)

**调度策略**: 优先级抢占式 + 同优先级时间片轮转

- 始终运行最高优先级的就绪任务
- 同优先级任务通过 SysTick 中断实现时间片轮转 (默认 5 ticks)
- 通过 PendSV 异常执行上下文切换

**关键机制**:
- `os_sched_enter_critical()` / `os_sched_exit_critical()`: 临界区保护 (关中断 + 嵌套计数)
- `os_sched_yield()`: 主动让出 CPU
- `os_sched_select_next()`: 选择下一个运行任务

### 4. Port 层 (`port/port.c`)

**ARM Cortex-M3 特定实现**:

**栈帧初始化** (模拟中断后的寄存器状态):
```
高地址
+----------------+
| xPSR           |  <-- 初始值 0x01000000 (Thumb模式)
| PC             |  <-- 任务入口函数地址
| LR             |  <-- 0xFFFFFFFE
| R12            |
| R3             |
| R2             |
| R1             |
| R0             |  <-- 任务参数
+----------------+  <-- 硬件自动保存 (8 words)
| R11            |
| R10            |
| R9             |
| R8             |
| R7             |
| R6             |
| R5             |
| R4             |
+----------------+  <-- 软件保存 (8 words)
低地址
```

**PendSV 上下文切换流程**:
1. 关中断
2. 保存 R4-R11 到当前任务栈
3. 更新当前 TCB 的 stack_ptr
4. 调用调度器选择下一个任务
5. 从新任务 TCB 加载 stack_ptr
6. 恢复 R4-R11
7. 开中断
8. 返回到新任务

**SysTick 配置**: 72MHz / 1000Hz = 72000 重载值

### 5. 内核核心 (`kernel/kernel.c`)

**初始化顺序**:
1. `os_heap_init()` - 初始化堆
2. `os_task_init_ready_list()` - 初始化就绪链表
3. `os_sched_init()` - 初始化调度器

**启动顺序**:
1. `os_task_create_idle()` - 创建空闲任务
2. `os_sched_select_next()` - 选择第一个任务
3. `os_port_start_first_task()` - 启动第一个任务 (永不返回)

---

## 搭建过程日志

### 第一阶段: 项目骨架搭建

**目标**: 建立目录结构和基础类型定义

1. 创建目录结构: `config/`, `common/`, `include/`, `kernel/`, `port/`, `app/`, `docs/`
2. 定义 `os_types.h` - 统一类型定义 (os_tick_t, os_prio_t, os_status_t 等)
3. 定义 `os_config.h` - 内核配置宏 (最大任务数、堆大小、tick频率等)
4. 定义 `os.h` - 对外统一入口头文件

### 第二阶段: Heap-4 内存管理

**目标**: 实现 FreeRTOS 风格的堆内存分配器

1. 设计 Block Header 结构 (size + allocated标记 + next指针)
2. 实现 `os_heap_init()` - 初始化单个大空闲块
3. 实现 `os_heap_alloc()` - 首次适配算法 + 块分割
4. 实现 `os_heap_free()` - 标记释放 + 相邻块合并
5. 实现统计接口 (free_size, largest_block, min_free)

**设计决策**:
- 使用 `size` 字段最高位作为分配标记，节省空间
- 空闲链表按地址排序，这是合并算法的前提
- 堆内存池使用 `__attribute__((aligned(8)))` 确保对齐

### 第三阶段: 任务管理

**目标**: 实现 TCB 管理、就绪链表、任务生命周期

1. 设计 TCB 结构 (栈指针、优先级、状态、链表指针)
2. 实现每优先级双向链表就绪队列
3. 实现 `os_task_create()` - 分配 TCB 和栈、初始化栈帧、加入就绪链表
4. 实现任务状态转换 (suspend/resume/delay)
5. 实现空闲任务 (WFI 低功耗循环)
6. 实现栈高水位检测 (0xA5A5A5A5 填充模式)

### 第四阶段: 调度器

**目标**: 实现优先级抢占式调度 + 时间片轮转

1. 实现 `os_sched_select_next()` - 扫描最高优先级就绪任务
2. 实现临界区 (关中断 + 嵌套计数)
3. 实现时间片轮转 (同优先级任务轮转)
4. 实现 `os_task_tick()` - 处理延时任务唤醒
5. 实现 `os_sched_yield()` - 主动让出 CPU

### 第五阶段: Port 层 (ARM Cortex-M3 移植)

**目标**: 实现硬件相关的上下文切换和中断处理

1. 实现 `os_port_stack_init()` - 构造初始栈帧
2. 实现 `PendSV_Handler()` - 上下文切换 (嵌入式汇编)
3. 实现 `SysTick_Handler()` - 系统时钟中断
4. 实现 `os_port_systick_init()` - 配置 SysTick 定时器
5. 实现 `os_port_start_first_task()` - 启动第一个任务
6. 编写启动文件 `startup_stm32f103.s` (向量表、.data/.bss 初始化)
7. 编写链接脚本 `stm32f103.ld` (Flash/RAM 布局)

### 第六阶段: 应用示例 & 构建系统

**目标**: 编写演示程序和 Makefile

1. 编写 `main.c` - 3个不同优先级的任务演示
2. 编写 `Makefile` - arm-none-eabi-gcc 交叉编译
3. 调试编译错误，修复符号冲突 (`current_task_ptr` 统一到 port.c)

---

## 构建说明

### 工具链要求

- `arm-none-eabi-gcc` (GNU ARM Embedded Toolchain)
- `make` (GNU Make)

### 编译

```bash
cd D:\A_stm32_project
make all
```

### 清理

```bash
make clean
```

### 烧录 (需要 OpenOCD + ST-Link)

```bash
make flash
```

### 输出文件

| 文件 | 说明 |
|------|------|
| `build/minios.elf` | ELF 可执行文件 (带调试信息) |
| `build/minios.hex` | Intel HEX 格式 (用于烧录) |
| `build/minios.bin` | 纯二进制 (用于烧录) |
| `build/minios.map` | 链接映射文件 |
| `build/minios.lst` | 反汇编列表 |

---

## 构建报告

### 编译单元

| 源文件 | 说明 | 依赖 |
|--------|------|------|
| `kernel/heap4.c` | Heap-4 内存管理 | os_config.h, os_types.h |
| `kernel/task.c` | 任务管理 | heap4.h, scheduler.h, port.h |
| `kernel/scheduler.c` | 调度器 | task.h, kernel.h, port.h |
| `kernel/kernel.c` | 内核核心 | heap4.h, task.h, scheduler.h, port.h |
| `port/port.c` | Cortex-M3 移植层 | task.h, os_config.h |
| `app/main.c` | 应用入口 | os.h |
| `port/startup_stm32f103.s` | 启动汇编 | stm32f103.ld |

### 内存占用预估

| 区域 | 大小 | 说明 |
|------|------|------|
| 代码 (.text) | ~2-3 KB | 内核 + 应用代码 |
| 常量 (.rodata) | ~100 B | 版本字符串等 |
| 数据 (.data) | ~50 B | 已初始化全局变量 |
| BSS (.bss) | ~16.5 KB | 堆池 (16KB) + 其他 |
| 栈 | ~1.5 KB | 3个任务栈 (512B x 3) |
| **总计** | **~20 KB** | RAM 使用接近上限 |

### 资源预算

- **Flash**: 64KB, 预计使用 ~3KB (4.7%)
- **RAM**: 20KB, 预计使用 ~18KB (90%) - 主要是堆池和任务栈
- **最大任务数**: 16
- **堆大小**: 16KB (可配置)

---

## 配置调优

编辑 `config/os_config.h` 修改系统参数:

```c
#define OS_CONFIG_MAX_TASKS          16     // 最大任务数
#define OS_CONFIG_TICK_RATE_HZ       1000   // 系统节拍 (1ms)
#define OS_CONFIG_DEFAULT_STACK_SIZE  512   // 默认栈大小
#define OS_CONFIG_HEAP_SIZE           (16*1024)  // 堆大小
#define OS_CONFIG_NUM_PRIORITIES      8     // 优先级数
#define OS_CONFIG_USE_TIME_SLICING    1     // 启用时间片轮转
#define OS_CONFIG_TIME_SLICE_TICKS    5     // 时间片长度 (5ms)
```

---

## 后续扩展方向

1. **同步原语**: 互斥锁 (Mutex)、信号量 (Semaphore)、事件标志组
2. **IPC**: 消息队列 (Queue)、邮箱 (Mailbox)
3. **软件定时器**: 基于 tick 的回调定时器
4. **中断管理**: 优先级分组、中断嵌套
5. **内存池**: 固定大小块分配器 (pool allocator)
6. **调试工具**: 任务状态查看、栈溢出检测、运行时统计
7. **低功耗**: Tickless Idle 模式
8. **移植**: 扩展到 Cortex-M0/M4/M7

---

## 已知问题与注意事项

1. **PendSV_Handler 内联汇编**: 当前使用 GCC 内联汇编实现，正式项目建议使用独立的 `.s` 文件
2. **中断优先级**: STM32 使用 4 位优先级，需确保 PendSV 设为最低优先级
3. **栈对齐**: ARM AAPCS 要求 8 字节栈对齐，`os_port_stack_init` 已处理
4. **current_task_ptr**: 全局变量在 port.c 中定义，供汇编代码直接访问
5. **编译器优化**: `-O2` 下内联汇编中的变量引用可能需要 `volatile` 修饰
