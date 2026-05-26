# MiniRTOS 技术设计文档

**版本**: v0.6.0
**目标平台**: ARM Cortex-M (STM32F103/F030/F407/F746)
**语言**: C11 + ARM Thumb-2 Assembly

---

## 目录

1. [设计理念与设计思路](#1-设计理念与设计思路)
2. [系统架构总览](#2-系统架构总览)
3. [目录结构与模块划分](#3-目录结构与模块划分)
4. [类型系统与配置框架](#4-类型系统与配置框架)
5. [内核核心 (kernel.c)](#5-内核核心-kernelc)
6. [任务管理 (task.c)](#6-任务管理-taskc)
7. [调度器 (scheduler.c)](#7-调度器-schedulerc)
8. [Heap-4 内存管理 (heap4.c)](#8-heap-4-内存管理-heap4c)
9. [硬件抽象层 / Port 层 (port.c)](#9-硬件抽象层--port-层-portc)
10. [启动文件与链接脚本](#10-启动文件与链接脚本)
11. [上下文切换完整流程分析](#11-上下文切换完整流程分析)
12. [API 参考手册](#12-api-参考手册)
13. [构建系统](#13-构建系统)
14. [内存布局与资源预算](#14-内存布局与资源预算)
15. [已知限制与注意事项](#15-已知限制与注意事项)
16. [后续扩展方向](#16-后续扩展方向)
17. [内存池分配器 (mempool.c)](#17-内存池分配器-mempoolc)

---

## 1. 设计理念与设计思路

### 1.1 为什么要做 MiniRTOS

嵌入式实时操作系统 (RTOS) 是现代嵌入式系统的核心基础设施。FreeRTOS、RT-Thread、Zephyr 等成熟的 RTOS 功能强大，但代码量庞大（FreeRTOS 内核约 10000+ 行），对于初学者而言，理解其全部内核机制的门槛很高。

MiniRTOS 的设计目标是：**用最少的代码实现一个可运行的 RTOS 内核骨架**，让开发者能够从零理解以下核心问题：

- 任务是什么？CPU 如何在多个任务之间切换？
- 调度器如何决定下一个运行的任务？
- 任务的延时 (delay) 是如何实现的？
- 内存是如何管理的？
- 中断和上下文切换的硬件机制是什么？

### 1.2 设计原则

**原则一：参考 FreeRTOS 架构，但只保留核心骨架**

MiniRTOS 的模块划分、数据结构设计、调度策略均参考 FreeRTOS v10.x。例如 Heap-4 的块分配算法、PendSV 上下文切换机制、每优先级链表的就绪队列等，都与 FreeRTOS 保持一致的设计思路。但 MiniRTOS 去掉了所有非核心功能（互斥锁、信号量、队列、软件定时器、协程等），只保留了最精简的"任务调度 + 内存管理"骨架。

**原则二：代码即文档**

每个函数都有清晰的注释说明其作用和设计意图。关键数据结构（如 TCB、Block Header）的设计决策在注释中解释，而不是分散到外部文档中。

**原则三：硬件相关代码严格隔离**

所有与 ARM Cortex-M3 硬件相关的代码（PendSV、SysTick、寄存器操作、内联汇编）全部封装在 `port/` 目录下。内核核心代码（`kernel/`）完全不包含任何硬件特定代码，理论上可以移植到任何架构。

**原则四：可配置、可裁剪**

所有可调参数集中在 `config/os_config.h` 中，通过宏定义控制。例如任务数、堆大小、时间片长度、是否启用时间片轮转等，都可以通过修改配置宏来调整，无需修改内核代码。

### 1.3 整体设计思路

MiniRTOS 的设计思路可以用一句话概括：**用定时器中断驱动调度，用链表管理任务，用堆分配器管理内存**。

具体来说：

1. **任务是基本执行单元**。每个任务有自己的栈空间和任务控制块 (TCB)，TCB 中记录了任务的栈指针、优先级、状态等信息。
2. **调度器是核心**。它维护一个按优先级组织的就绪链表，始终选择最高优先级的就绪任务来运行。
3. **SysTick 定时器是时间源**。每 1ms 产生一次中断，中断处理函数递增系统 tick、处理延时任务的唤醒、触发上下文切换。
4. **PendSV 异常是上下文切换的执行者**。它是 Cortex-M3 中优先级最低的异常，确保上下文切换发生在所有其他中断处理完成之后。
5. **Heap-4 分配器管理动态内存**。采用首次适配算法 + 相邻空闲块合并，平衡了分配效率和内存碎片控制。

---

## 2. 系统架构总览

```
+-------------------------------------------------------+
|                    应用层 (app/)                        |
|  main.c - 用户任务代码                                  |
|  只需 #include "os.h" 即可使用全部 OS 功能              |
+-------------------------------------------------------+
                           |
                           | API 调用
                           v
+-------------------------------------------------------+
|                 OS 公共接口 (include/os.h)              |
|  统一入口头文件，聚合所有子模块 API                      |
|  便捷宏: OS_DELAY_MS(), OS_ENTER_CRITICAL() 等         |
+-------------------------------------------------------+
                           |
            +--------------+--------------+
            |              |              |
            v              v              v
+----------------+ +----------------+ +----------------+
| 任务管理       | |  调度器        | |  内核核心      |
| kernel/task.c  | | kernel/sched.c | | kernel/kernel.c|
| - TCB 管理     | | - 优先级抢占   | | - 初始化       |
| - 就绪链表     | | - 时间片轮转   | | - tick 管理    |
| - 任务生命周期 | | - 临界区       | | - assert       |
+----------------+ +----------------+ +----------------+
            |              |              |
            v              v              v
+-------------------------------------------------------+
|              Heap-4 内存管理 (kernel/heap4.c)           |
|  首次适配 + 相邻空闲块合并                              |
|  10KB 静态堆池                                          |
+-------------------------------------------------------+
                           |
                           | 硬件抽象
                           v
+-------------------------------------------------------+
|              Port 层 (port/port.c)                     |
|  - 栈帧初始化 (os_port_stack_init)                     |
|  - PendSV 上下文切换 (内联汇编)                        |
|  - SysTick 定时器配置与中断处理                        |
|  - 临界区 (PRIMASK 关中断)                             |
+-------------------------------------------------------+
                           |
                           v
+-------------------------------------------------------+
|              硬件 (ARM Cortex-M3)                      |
|  - SysTick 定时器                                      |
|  - PendSV 异常                                         |
|  - PSP/MSP 双栈指针                                    |
|  - NVIC 中断控制器                                     |
+-------------------------------------------------------+
```

**层间依赖关系**：

- 应用层只依赖 `os.h`，不直接访问内核内部
- 内核模块（task、scheduler、kernel）通过头文件互相引用，但不包含硬件代码
- Port 层被内核通过函数调用间接使用，内核不直接操作硬件寄存器
- 所有模块共享 `os_types.h`（类型定义）和 `os_config.h`（配置宏）

---

## 3. 目录结构与模块划分

```
miniRTOS/
├── config/
│   └── os_config.h           # 内核配置: 任务数、堆大小、tick 频率、时间片等
├── common/
│   └── os_types.h            # 通用类型: 状态码、任务句柄、任务状态枚举
├── include/
│   └── os.h                  # 对外统一头文件 + 便捷宏定义
├── kernel/
│   ├── heap4.h / heap4.c     # Heap-4 动态内存分配器
│   ├── task.h / task.c       # 任务管理: TCB、就绪链表、任务生命周期
│   ├── scheduler.h / scheduler.c  # 调度器: 优先级抢占 + 时间片轮转
│   └── kernel.h / kernel.c   # 内核初始化与系统 tick 管理
├── port/
│   ├── port.h / port.c       # Cortex-M3 移植层 (上下文切换、SysTick)
│   ├── startup_stm32f103.s   # 启动汇编 (向量表、.data/.bss 初始化)
│   └── stm32f103.ld          # 链接脚本 (Flash/RAM 布局)
├── app/
│   └── main.c                # 应用入口 (3 个示例任务)
├── docs/
│   └── miniRTOS_technical_design.md  # 本文档
└── Makefile                  # 构建系统
```

**模块职责划分**：

| 模块 | 文件 | 职责 | 代码行数 |
|------|------|------|---------|
| 类型定义 | `os_types.h` | 统一类型、状态码、枚举 | ~45 |
| 配置 | `os_config.h` | 所有可调参数 | ~49 |
| 公共接口 | `os.h` | 聚合头文件 + 便捷宏 | ~39 |
| 内存管理 | `heap4.c/h` | 堆分配/释放/统计 | ~310 |
| 任务管理 | `task.c/h` | TCB/就绪链表/任务生命周期/自删除/栈溢出检测 | ~547 |
| 调度器 | `scheduler.c/h` | 优先级抢占/时间片/临界区 | ~122 |
| 内核核心 | `kernel.c/h` | 初始化/tick/assert | ~93 |
| Port 层 | `port.c/h` | 上下文切换/SysTick/临界区 | ~284 |
| 启动文件 | `startup_stm32f103.s` | 向量表/Reset_Handler | ~266 |
| 链接脚本 | `stm32f103.ld` | Flash/RAM 内存布局 | ~147 |
| 应用 | `main.c` | 示例任务 | ~77 |
| **合计** | | | **~1984** |

---

## 4. 类型系统与配置框架

### 4.1 统一类型定义 (`common/os_types.h`)

所有模块共用的基础类型，保证接口一致性：

```c
typedef uint32_t    os_tick_t;          // 系统 tick 计数
typedef uint32_t    os_prio_t;          // 优先级 (0 = 最高)
typedef uint32_t    os_stack_t;         // 栈元素 (32-bit word)
typedef void*       os_task_handle_t;   // 任务句柄 (指向 TCB)
typedef void        (*os_task_func_t)(void *param);  // 任务入口函数指针
```

**设计决策**：

- `os_stack_t` 使用 `uint32_t`（4 字节），因为 Cortex-M3 的寄存器是 32 位的，栈操作以 word 为单位
- `os_task_handle_t` 使用 `void*` 而非 `os_tcb_t*`，对外隐藏 TCB 内部结构，实现信息隐藏
- `os_task_func_t` 统一为 `void(*)(void*)`，参数为 `void*` 以支持任意数据传递

### 4.2 返回状态码

```c
typedef enum {
    OS_OK           = 0,    // 成功
    OS_ERR_NOMEM    = -1,   // 内存不足
    OS_ERR_PARAM    = -2,   // 参数错误
    OS_ERR_STATE    = -3,   // 状态错误
    OS_ERR_FULL     = -4,   // 任务表满
    OS_ERR_EMPTY    = -5,   // 空
    OS_ERR_TIMEOUT  = -6,   // 超时
} os_status_t;
```

### 4.3 任务状态机

```c
typedef enum {
    OS_TASK_READY      = 0,  // 就绪: 在就绪链表中等待调度
    OS_TASK_RUNNING    = 1,  // 运行: 正在占用 CPU
    OS_TASK_SUSPENDED  = 2,  // 挂起: 被显式挂起，需 resume 唤醒
    OS_TASK_BLOCKED    = 3,  // 阻塞: 等待延时到期
    OS_TASK_DELETED    = 4,  // 已删除
} os_task_state_t;
```

**状态转换图**：

```
                    os_task_create()
                         |
                         v
    +---------->  READY <--------+--------+
    |               |             |        |
    |  调度器选中   |  os_task_   |  延时  |
    |               |  resume()   |  到期  |
    |               v             |        |
    |           RUNNING          |        |
    |               |             |        |
    |  被抢占       | os_task_    |        |
    |  (时间片/     | suspend()   |        |
    |   更高优先级) |             |        |
    |               v             |        |
    |          SUSPENDED          |        |
    |                             |        |
    |        os_task_delay()      |        |
    |               |             |        |
    |               v             |        |
    +-------- BLOCKED ------------+        |
                    |                      |
                    | os_task_delete()     |
                    v                      |
                 DELETED                   |
                    |                      |
                    +--- 资源回收 ----------+
```

### 4.4 配置框架 (`config/os_config.h`)

所有可调参数集中管理：

```c
// 任务相关
#define OS_CONFIG_MAX_TASKS             16      // 最大任务数 (含空闲任务)
#define OS_CONFIG_TICK_RATE_HZ          1000    // 系统 tick 频率 (1ms 精度)
#define OS_CONFIG_DEFAULT_STACK_SIZE    512     // 默认栈大小 (字节)
#define OS_CONFIG_IDLE_STACK_SIZE       256     // 空闲任务栈大小
#define OS_CONFIG_MAX_NAME_LEN          16      // 任务名最大长度
#define OS_CONFIG_NUM_PRIORITIES        8       // 优先级数 (0=最高)

// 时间片相关
#define OS_CONFIG_USE_TIME_SLICING      1       // 启用同优先级时间片轮转
#define OS_CONFIG_TIME_SLICE_TICKS      5       // 时间片长度 (5 ticks = 5ms)

// 堆相关
#define OS_CONFIG_HEAP_SIZE             (10*1024)   // 堆大小 10KB
#define OS_CONFIG_HEAP_ALIGNMENT        8       // 堆对齐粒度

// 调试
#define OS_CONFIG_ASSERT_ENABLE         1       // 启用断言
#define OS_CONFIG_DEBUG_LOG_ENABLE      1       // 启用调试日志
#define OS_CONFIG_STACK_OVERFLOW_CHECK  1       // 启用栈溢出检测 (canary)
```

**配置调优指南**：

| 参数 | 增大影响 | 减小影响 |
|------|---------|---------|
| `MAX_TASKS` | 占用更多 RAM (task_table) | 可创建任务数减少 |
| `TICK_RATE_HZ` | 调度更精细，CPU 开销增大 | 调度粒度变粗 |
| `HEAP_SIZE` | 可分配更多内存 | 占用更少 RAM |
| `NUM_PRIORITIES` | 调度更灵活 | 扫描更快 |
| `TIME_SLICE_TICKS` | 同优先级任务切换更少 | 切换更频繁 |

---

## 5. 内核核心 (kernel.c)

### 5.1 职责

`kernel.c` 是整个操作系统的入口点，负责：

1. **初始化顺序控制**：按正确顺序初始化所有子系统
2. **系统 tick 管理**：维护全局 tick 计数器
3. **启动调度器**：调用调度器开始多任务运行
4. **断言处理**：提供统一的断言失败处理机制

### 5.2 初始化流程

```c
void os_kernel_init(void)
{
    os_heap_init();                // 1. 初始化堆分配器
    os_task_init_ready_list();     // 2. 初始化就绪链表和任务表
    os_sched_init();               // 3. 初始化调度器状态
    kernel_initialized = true;
}
```

**设计决策**：初始化顺序很重要。堆必须最先初始化，因为后续的任务创建（TCB 和栈分配）依赖堆。就绪链表必须在调度器之前初始化，因为调度器依赖就绪链表来选择任务。

### 5.3 启动流程

```c
void os_kernel_start(void)
{
    os_port_systick_init(OS_CONFIG_TICK_RATE_HZ);  // 1. 配置 SysTick 定时器
    os_sched_start();                               // 2. 启动调度器 (永不返回)
}
```

`os_kernel_start()` 永不返回。它启动 SysTick 定时器后，调用 `os_sched_start()`，后者创建空闲任务、选择第一个任务、启动上下文切换，CPU 永远在任务之间轮转。

### 5.4 Tick 中断处理

```c
void os_kernel_tick_increment(void)
{
    system_tick++;           // 递增全局 tick
    os_task_tick();          // 处理延时任务 (唤醒到期的 BLOCKED 任务)
    os_sched_select_next();  // 选择下一个要运行的任务 (可能触发上下文切换)
}
```

这个函数由 `SysTick_Handler()` 调用，是整个操作系统的"心跳"。每次 SysTick 中断（1ms）都会执行一次。

### 5.5 断言机制

```c
void os_assert_failed(const char *file, uint32_t line)
{
    os_sched_enter_critical();          // 关中断
    os_port_debug_print("ASSERT FAILED: ");
    os_port_debug_print(file);
    while (1) {
        __asm volatile("bkpt #0");      // 触发断点，便于调试器捕获
    }
}
```

通过 `OS_ASSERT(expr)` 宏使用，当条件为 false 时调用。断言失败后系统停止运行，`bkpt` 指令会让调试器停在断言处，方便定位问题。

---

## 6. 任务管理 (task.c)

### 6.1 任务控制块 (TCB)

TCB 是任务的核心数据结构，存储了任务运行所需的全部信息：

```c
typedef struct os_tcb {
    os_stack_t      *stack_ptr;         // 当前栈指针 (必须在结构体首位!)
    os_stack_t      *stack_base;        // 栈底地址 (用于释放)
    uint32_t        stack_size;         // 栈大小 (字节)
    os_prio_t       priority;           // 优先级 (0=最高)
    os_task_state_t state;              // 当前状态
    os_tick_t       delay_ticks;        // 阻塞时剩余 tick 数
    char            name[16];           // 任务名 (调试用)
    os_task_func_t  entry_func;         // 入口函数指针
    void            *param;             // 入口参数
    struct os_tcb   *next;              // 链表后向指针
    struct os_tcb   *prev;              // 链表前向指针
    uint8_t         pending_delete;     // 延迟删除标志 (自删除时使用)
    uint32_t        stack_high_water;   // 栈使用峰值 (字节)
} os_tcb_t;
```

**关键设计点**：

1. **`stack_ptr` 必须在首位**：PendSV 的汇编代码通过 `current_task_ptr` 直接访问 TCB 的第一个字段来获取/保存栈指针。如果 `stack_ptr` 不在偏移 0 的位置，汇编代码就会读取错误的数据。

2. **双向链表**：`next` 和 `prev` 指针使得从链表中删除任意节点的操作是 O(1) 的，不需要遍历链表找前驱节点。

3. **栈高水位**：通过 `0xA5A5A5A5` 填充模式来检测栈的实际使用量。任务运行过程中，栈会从高地址向低地址增长，覆盖掉部分 `0xA5A5A5A5` 模式。通过检查从栈底开始有多少连续的 `0xA5A5A5A5`，就可以计算出栈的实际使用峰值。

### 6.2 就绪链表设计

```c
typedef struct {
    os_tcb_t *head;     // 链表头
    os_tcb_t *tail;     // 链表尾
    uint32_t count;     // 任务计数
} os_ready_list_t;

static os_ready_list_t ready_list[OS_CONFIG_NUM_PRIORITIES];
```

**设计思路**：使用"每优先级一个链表"的结构，而非全局排序链表。

- **优点**：插入和删除都是 O(1) 操作（直接挂到对应优先级链表的尾部），查找最高优先级任务只需从 priority 0 开始扫描，找到第一个非空链表即可。
- **对比全局排序链表**：如果使用单个全局链表按优先级排序，插入时需要 O(n) 遍历找到正确位置。

**就绪链表操作**：

```c
// 插入到就绪链表尾部 (同优先级 FIFO 顺序)
void os_task_add_to_ready(os_tcb_t *tcb)
{
    tcb->state = OS_TASK_READY;
    tcb->next = NULL;
    tcb->prev = ready_list[prio].tail;

    if (ready_list[prio].tail != NULL)
        ready_list[prio].tail->next = tcb;
    else
        ready_list[prio].head = tcb;

    ready_list[prio].tail = tcb;
    ready_list[prio].count++;
}
```

### 6.3 任务创建

```c
os_status_t os_task_create(
    os_task_func_t func,        // 入口函数
    const char *name,           // 任务名
    void *param,                // 入口参数
    os_prio_t priority,         // 优先级
    os_stack_t *stack_buf,      // 栈缓冲区 (NULL=动态分配)
    uint32_t stack_size,        // 栈大小 (字节)
    os_task_handle_t *handle    // 输出: 任务句柄
)
```

**创建流程**：

1. **参数校验**：检查函数指针非空、优先级合法、栈大小 >= 128 字节
2. **分配栈**：如果 `stack_buf` 为 NULL，从堆中动态分配；否则使用用户提供的静态栈
3. **分配 TCB**：从堆中分配 TCB 结构体
4. **填充栈模式**：用 `0xA5A5A5A5` 填充栈空间，用于后续的高水位检测
5. **初始化 TCB 字段**：设置栈底、栈大小、优先级、状态、入口函数等
6. **构造初始栈帧**：调用 `os_port_stack_init()` 在栈中构造模拟中断后的寄存器状态
7. **加入任务表和就绪链表**：TCB 指针存入 `task_table[]`，并加入对应优先级的就绪链表

**栈分配策略**：

MiniRTOS 支持两种栈分配方式：
- **动态分配**：`stack_buf = NULL`，由 `os_heap_alloc()` 从堆中分配。灵活但会产生碎片。
- **静态分配**：用户传入预分配的数组。无碎片，栈大小在编译时确定。

推荐在嵌入式系统中使用静态分配，因为：
- 避免堆碎片导致分配失败
- 栈大小在编译时确定，便于分析内存使用
- 不依赖堆分配器的正确性

### 6.4 任务延时

```c
os_status_t os_task_delay(os_tick_t ticks)
```

**延时实现原理**：

1. 将当前任务从就绪链表中移除
2. 设置 `delay_ticks` 为延时 tick 数
3. 将任务加入 `blocked_list`（阻塞链表，单向链表，O(1) 插入到头部）
4. 触发上下文切换（`os_sched_yield()`）

**唤醒机制**：在 `os_task_tick()` 中，遍历 `blocked_list`，每个任务的 `delay_ticks` 减 1，减到 0 时从阻塞链表移除，重新加入就绪链表。

```c
void os_task_tick(void)
{
    os_tcb_t *tcb = blocked_list;
    while (tcb != NULL) {
        os_tcb_t *next = tcb->next;
        if (tcb->delay_ticks > 0) {
            tcb->delay_ticks--;
            if (tcb->delay_ticks == 0) {
                // 从阻塞链表移除
                // 加入就绪链表
                os_task_add_to_ready(tcb);
            }
        }
        tcb = next;
    }
}
```

### 6.5 空闲任务

```c
static void idle_task_func(void *param)
{
    (void)param;
    while (1) {
        __asm volatile("wfi");  // Wait For Interrupt，进入低功耗模式
    }
}
```

空闲任务运行在最低优先级 (`OS_PRIO_LOWEST`)，只有当所有其他任务都在阻塞/挂起状态时才会执行。`wfi` 指令让 CPU 进入睡眠模式，直到下一个中断到来，实现低功耗。

### 6.6 任务自删除 (Deferred Delete)

**问题**：当一个任务调用 `os_task_delete(NULL)` 删除自己时，CPU 正在该任务的栈上执行代码。如果立即释放栈内存，后续代码会访问已释放的内存，导致数据损坏或崩溃。

**解决方案**：延迟释放 (Deferred Delete)

```c
os_status_t os_task_delete(os_task_handle_t handle)
{
    if (handle == NULL) {
        // 自删除: 不能立即释放自己的栈
        tcb = current_task_ptr;

        // 1. 从就绪链表移除
        os_task_remove_from_ready(tcb);
        tcb->state = OS_TASK_DELETED;
        tcb->pending_delete = 1;

        // 2. 加入延迟删除链表
        tcb->next = deferred_delete_list;
        deferred_delete_list = tcb;

        // 3. 触发上下文切换 (切换到其他任务后，栈不再使用)
        os_sched_yield();
        return OS_OK;
    }

    // 删除其他任务: 可以立即释放
    // ...
}
```

**延迟释放机制**：

```
任务 A 调用 os_task_delete(NULL)
    |
    v
A 被标记为 pending_delete，加入 deferred_delete_list
    |
    v
os_sched_yield() 触发上下文切换
    |
    v
PendSV 切换到任务 B (A 的栈不再使用)
    |
    v
下一个 SysTick 中断
    |
    v
os_task_tick() → os_task_process_deferred_delete()
    |
    v
遍历 deferred_delete_list，释放每个任务的栈和 TCB
```

**关键设计点**：

1. `deferred_delete_list` 使用 TCB 自身的 `next` 指针串联，不需要额外的链表节点
2. 资源释放在 `os_task_tick()` 中执行，此时调度器已切换到其他任务，被删除任务的栈已不再使用
3. 空闲任务的栈是静态分配的 (`idle_task_stack[]`)，释放时会跳过
4. `pending_delete` 标志位可用于调试时判断任务是否处于待删除状态

### 6.7 创建挂起态任务

```c
os_status_t os_task_create_suspended(
    os_task_func_t func,
    const char *name,
    void *param,
    os_prio_t priority,
    os_stack_t *stack_buf,
    uint32_t stack_size,
    os_task_handle_t *handle
)
```

**实现原理**：先调用 `os_task_create()` 创建任务（进入 READY 状态），再立即调用 `os_task_suspend()` 将其移入 SUSPENDED 状态。

**使用场景**：

```c
// 创建任务但不立即运行，等待某个条件后再 resume
os_task_handle_t handle;
os_task_create_suspended(task_func, "WORK", NULL, 3,
                         stack_buf, sizeof(stack_buf), &handle);

// ... 某个条件满足后 ...
os_task_resume(handle);  // 此时任务才开始运行
```

**设计决策**：选择"创建 + 立即挂起"的实现方式，而非在 `os_task_create()` 中添加 `initial_state` 参数，原因是：
- 保持 `os_task_create()` 的接口简洁
- 避免修改已有 API 签名，保持向后兼容
- 复用已有的 `os_task_suspend()` 逻辑

### 6.8 栈溢出检测

**检测原理**：在栈的最低地址处写入一个已知的标记值 (canary)，每次 tick 中断时检查该标记是否被破坏。

```
栈内存布局 (低地址在下):
+------------------+  ← 栈顶 (高地址)
| 任务数据         |
| ...              |
| 栈增长方向 ↓     |
| ...              |
+------------------+  ← stack_base[1]
| Canary (0xCCCCCCCC) |  ← stack_base[0] (最低地址)
+------------------+  ← 栈底
```

**实现**：

```c
// 初始化时写入 canary
static void prv_fill_stack(os_stack_t *stack, uint32_t size)
{
    // 填充高水位标记模式
    for (uint32_t i = 0; i < words; i++)
        stack[i] = 0xA5A5A5A5;

    // 在栈底写入 canary
    stack[0] = OS_STACK_CANARY_VALUE;  // 0xCCCCCCCC
}

// 每次 tick 检查 canary
void os_task_check_stack_overflow(void)
{
    if (current_task_ptr->stack_base[0] != OS_STACK_CANARY_VALUE) {
        os_assert_failed(__FILE__, __LINE__);  // 栈溢出!
    }
}
```

**配置控制**：

```c
// os_config.h
#define OS_CONFIG_STACK_OVERFLOW_CHECK  1   // 1=启用, 0=禁用
```

**检测能力与限制**：

| 能力 | 说明 |
|------|------|
| 可检测 | 栈增长覆盖了 `stack_base[0]` 的 canary 值 |
| 不可检测 | 栈溢出到其他内存区域但未覆盖 canary（概率极低） |
| 不可检测 | 数组越界访问（不是栈溢出） |
| 性能开销 | 每次 tick 中断一次 word 比较，可忽略 |

### 6.9 任务查询 API

```c
// 查询任务当前状态
os_task_state_t os_task_get_state(os_task_handle_t handle);

// 查询任务优先级
os_prio_t os_task_get_priority(os_task_handle_t handle);

// 获取当前任务总数 (含空闲任务)
uint32_t os_task_get_count(void);

// 获取栈使用峰值 (已有)
uint32_t os_task_get_stack_high_water(os_task_handle_t handle);
```

**使用示例**：

```c
os_task_handle_t handle = os_task_get_current();

// 查询当前任务状态
os_task_state_t state = OS_TASK_GET_STATE(handle);
// state == OS_TASK_RUNNING

// 查询任务优先级
os_prio_t prio = OS_TASK_GET_PRIO(handle);

// 查询系统中有多少任务
uint32_t count = OS_TASK_GET_COUNT();

// 查询栈使用情况 (用于调优栈大小)
uint32_t used = os_task_get_stack_high_water(handle);
// 如果 used 接近 stack_size，说明栈可能不够大
```

---

## 7. 调度器 (scheduler.c)

### 7.1 调度策略

MiniRTOS 采用**优先级抢占式调度 + 同优先级时间片轮转**的混合策略：

**优先级抢占**：始终运行就绪状态中优先级最高的任务。当一个更高优先级的任务变为就绪（例如从 BLOCKED 变为 READY），它会立即抢占当前正在运行的低优先级任务。

**时间片轮转**：当多个任务具有相同优先级时，它们轮流执行，每个任务运行一个时间片（默认 5 ticks = 5ms）后被切换到同优先级链表的尾部，下一个同优先级任务获得 CPU。

### 7.2 任务选择算法

```c
void os_sched_select_next(void)
{
    os_tcb_t *next_task = os_task_find_highest_ready();
    os_tcb_t *current = os_task_get_current();

    if (next_task == current) {
#if OS_CONFIG_USE_TIME_SLICING
        // 同优先级轮转: 把当前任务移到链表尾部
        os_task_remove_from_ready(current);
        os_task_add_to_ready(current);
        next_task = os_task_find_highest_ready();
#else
        return;  // 不切换
#endif
    }

    // 更新状态
    if (current != NULL && current->state == OS_TASK_RUNNING)
        current->state = OS_TASK_READY;

    next_task->state = OS_TASK_RUNNING;
    os_task_set_current(next_task);
}
```

**时间片轮转的工作原理**：

1. SysTick 中断触发 → `os_kernel_tick_increment()` → `os_sched_select_next()`
2. `os_task_find_highest_ready()` 找到最高优先级就绪链表的 head
3. 如果 head 就是当前任务（同优先级唯一就绪任务），直接返回，不切换
4. 如果 head 是当前任务但链表中有其他同优先级任务，把当前任务移到链表尾部，新的 head 成为下一个运行任务
5. 更新 `current_task_ptr` 为新任务
6. `PendSV_Handler()` 执行实际的上下文切换

### 7.3 临界区管理

```c
static uint32_t critical_nesting = 0;

void os_sched_enter_critical(void)
{
    uint32_t mask = os_port_enter_critical();  // 关中断 (CPSID I)
    critical_nesting++;
}

void os_sched_exit_critical(void)
{
    if (critical_nesting > 0) {
        critical_nesting--;
        if (critical_nesting == 0) {
            os_port_exit_critical(0);  // 开中断 (MSR PRIMASK, ...)
        }
    }
}
```

**设计要点**：

- 临界区支持嵌套。`critical_nesting` 记录嵌套深度，只有最外层 `exit_critical` 才真正恢复中断。
- `os_port_enter_critical()` 通过 `PRIMASK` 寄存器禁用所有中断，返回之前的 `PRIMASK` 值。
- 临界区保护了所有对就绪链表和阻塞链表的操作，确保在多任务/中断环境下数据一致性。

### 7.4 ISR 中的上下文切换请求

```c
void os_sched_request_switch_from_isr(void)
{
    yield_pending = 1;
}
```

当 ISR 中需要触发上下文切换时（例如释放信号量唤醒了更高优先级的任务），ISR 不能直接调用 `os_sched_yield()`（因为 ISR 中不应直接切换上下文），而是设置 `yield_pending` 标志，在 ISR 返回后由硬件自动触发 PendSV 进行切换。

---

## 8. Heap-4 内存管理 (heap4.c)

### 8.1 算法概述

Heap-4 是 FreeRTOS 中最常用的堆分配算法，MiniRTOS 的实现与其设计思路一致：

- **分配**：首次适配 (First-Fit) — 从空闲链表中找到第一个足够大的块
- **释放**：标记为空闲后，自动与相邻的空闲块合并 (Coalescing)
- **链表**：空闲和已分配的块都在同一个链表中，按地址排序

### 8.2 内存布局

```
堆内存池 (10KB, 8字节对齐):
+--------+----------+--------+----------+--------+----------+
| Header | Data     | Header | Data     | Header | Data     |
| 16B    | N bytes  | 16B    | M bytes  | 16B    | K bytes  |
+--------+----------+--------+----------+--------+----------+
^                                                       ^
|                                                       |
px_block_list -----> next -----> next -----> next ------+
```

### 8.3 块头部结构

```c
typedef struct os_block_header {
    struct os_block_header *next;       // 下一个块指针
    uint32_t               size;        // 数据区大小 (最高位用作分配标记)
    uint8_t                allocated;   // 分配状态 (冗余字段)
    uint8_t                reserved[3]; // 对齐填充
} os_block_header_t;
```

**设计决策**：

- `size` 字段的最高位 (bit 31) 用作分配标记 (`BLOCK_ALLOCATED_BIT = 0x80000000`)，低 31 位存储实际大小。这样 `size` 字段同时承载了大小和状态信息，节省了空间。
- `allocated` 字段是冗余的（与 `size` 最高位重复），但提供了更方便的代码可读性。
- 块头部大小为 16 字节（8 字节对齐后），每个分配至少有 16 字节的开销。

### 8.4 分配算法 (os_heap_alloc)

```c
void* os_heap_alloc(uint32_t size)
{
    // 1. 对齐请求大小到 8 字节边界
    size = ALIGN_UP(size);

    // 2. 首次适配: 遍历链表找到第一个足够大的空闲块
    for (block = px_block_list; block != NULL; block = block->next) {
        if (!prv_block_is_allocated(block) && prv_block_get_size(block) >= size)
            break;
    }

    // 3. 如果找到的块比请求大很多，进行分割
    if (block_size >= size + sizeof(header) + OS_CONFIG_HEAP_ALIGNMENT) {
        // 在块的尾部创建一个新的空闲块
        new_block = (header*)(data + size);
        new_block->size = block_size - size - sizeof(header);
        block->size = size;
        // 链入链表
    }

    // 4. 标记为已分配
    prv_block_set_allocated(block);
    return BLOCK_DATA(block);
}
```

**块分割条件**：只有当剩余空间 >= `sizeof(header) + OS_CONFIG_HEAP_ALIGNMENT`（16 + 8 = 24 字节）时才分割。如果剩余空间太小，分割出的新块无法使用，反而浪费了一个头部的空间。

### 8.5 释放与合并算法 (os_heap_free)

```c
void os_heap_free(void *ptr)
{
    // 1. 从链表中移除该块
    // 2. 标记为空闲
    prv_block_set_free(block);
    // 3. 重新插入链表 (按地址排序)
    prv_insert_block(block);
}
```

`prv_insert_block()` 是合并的核心：

```c
static void prv_insert_block(os_block_header_t *block_to_insert)
{
    // 1. 按地址顺序找到插入位置
    for (iter = px_block_list; iter != NULL; iter = iter->next) {
        if ((uint8_t*)iter > (uint8_t*)block_to_insert) break;
        prev = iter;
    }

    // 2. 插入到 prev 和 iter 之间
    block_to_insert->next = iter;
    prev->next = block_to_insert;

    // 3. 尝试与前一个空闲块合并
    if (prev 未分配 && prev 尾部紧邻 block_to_insert) {
        prev->size += sizeof(header) + block_to_insert->size;
        prev->next = block_to_insert->next;
        block_to_insert = prev;  // 合并后继续检查与后块的合并
    }

    // 4. 尝试与后一个空闲块合并
    if (block_to_insert->next 未分配 && block_to_insert 尾部紧邻 next) {
        block_to_insert->size += sizeof(header) + next->size;
        block_to_insert->next = next->next;
    }
}
```

**合并的关键前提**：空闲链表必须按地址排序。这样在插入一个块时，它的前驱和后继一定是内存地址上相邻的块，才能正确判断是否可以合并。

### 8.6 统计接口

| 函数 | 作用 | 时间复杂度 |
|------|------|-----------|
| `os_heap_get_free_size()` | 返回当前空闲字节数 | O(1) |
| `os_heap_get_min_free_size()` | 返回历史最小空闲字节数 | O(1) |
| `os_heap_get_largest_free_block()` | 返回最大连续空闲块 | O(n) |

`min_free_bytes_ever` 是内存使用的"高水位标记"，帮助开发者评估堆是否足够大。

---

## 9. 硬件抽象层 / Port 层 (port.c)

### 9.1 职责

Port 层是 MiniRTOS 与硬件之间的桥梁，封装了所有 ARM Cortex-M3 特定的操作：

1. **栈帧初始化** — 为新任务构造模拟中断后的栈帧
2. **PendSV 处理** — 执行实际的上下文切换
3. **SysTick 配置** — 设置系统定时器
4. **临界区** — 通过 PRIMASK 实现中断开关
5. **第一次任务启动** — 从裸机状态切换到多任务状态

### 9.2 ARM Cortex-M3 关键硬件机制

#### PSP/MSP 双栈指针

Cortex-M3 有两个栈指针：
- **MSP (Main Stack Pointer)**：中断/异常处理使用
- **PSP (Process Stack Pointer)**：线程模式（任务执行）使用

MiniRTOS 使用 PSP 作为任务栈指针，MSP 作为中断栈指针。这样每个任务有自己的 PSP，而所有中断共享一个 MSP。

#### PendSV 异常

PendSV (Pending Service) 是 Cortex-M3 中用于上下文切换的异常，关键特性：
- 优先级可以设置为最低 (0xFF)
- 通过写 SCB->ICSR 的 PENDSVSET 位触发
- 在所有其他中断处理完成后才执行

将其设为最低优先级的目的是：确保上下文切换发生在所有 ISR 执行完毕之后，避免在 ISR 中间切换上下文导致数据不一致。

#### SysTick 定时器

Cortex-M3 内置的 24 位倒计数定时器，配置为 1000Hz (1ms 周期) 产生中断，作为操作系统的时钟源。

### 9.3 栈帧初始化

当创建一个新任务时，需要在它的栈中构造一个"假的"中断栈帧，使得当这个任务第一次被调度运行时，CPU 恢复这个栈帧就像从一个真实的中断中返回一样。

```c
os_stack_t* os_port_stack_init(os_task_func_t func, void *param,
                                os_stack_t *stack_base, uint32_t stack_size)
{
    // 栈从高地址向低地址增长
    sp = (stack_base + stack_size) 对齐到 8 字节;

    // 硬件自动保存的寄存器 (中断返回时由 CPU 恢复)
    *(--sp) = 0x01000000;       // xPSR: Thumb 模式位
    *(--sp) = (uint32_t)func;   // PC: 任务入口函数
    *(--sp) = 0xFFFFFFFE;       // LR: 返回 Thread 模式
    *(--sp) = 0x12121212;       // R12
    *(--sp) = 0x03030303;       // R3
    *(--sp) = 0x02020202;       // R2
    *(--sp) = 0x01010101;       // R1
    *(--sp) = (uint32_t)param;  // R0: 任务参数

    // 软件保存的寄存器 (PendSV_Handler 中手动恢复)
    *(--sp) = 0x11111111;       // R11
    *(--sp) = 0x10101010;       // R10
    *(--sp) = 0x09090909;       // R9
    *(--sp) = 0x08080808;       // R8
    *(--sp) = 0x07070707;       // R7
    *(--sp) = 0x06060606;       // R6
    *(--sp) = 0x05050505;       // R5
    *(--sp) = 0x04040404;       // R4

    return sp;  // 返回新的栈顶指针
}
```

**为什么 R0-R3, R12 使用特殊值**：

这些值 (0x01010101, 0x02020202, ...) 不是随机的。它们是调试辅助值：
- 如果任务执行过程中某个寄存器异常地保留了这些值，说明它可能没有被正确初始化
- 这些值在调试器中很容易辨认，帮助定位问题

**xPSR = 0x01000000**：

xPSR 的 bit 24 是 Thumb 模式位。Cortex-M3 只支持 Thumb 指令集，这个位必须为 1，否则 CPU 会产生 UsageFault。

**PC = func**：

任务入口函数地址。当这个栈帧被"中断返回"时，CPU 会将这个值加载到 PC，跳转到任务函数开始执行。

**LR = 0xFFFFFFFE**：

这是一个特殊的 EXC_RETURN 值，告诉 CPU 从异常返回时使用 PSP（而不是 MSP），进入线程模式。

### 9.4 PendSV 上下文切换

这是整个操作系统中最关键、最复杂的代码，用内联汇编实现：

```c
void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void)
{
    __asm volatile(
        "cpsid i\n"                     // 1. 关中断

        "mrs r0, psp\n"                 // 2. 获取 PSP (任务栈指针)
        "tst r14, #0x10\n"              // 3. 检查是否使用了 FPU
        "it eq\n"
        "vstmdbeq r0!, {s16-s31}\n"     // 4. 如果有 FPU，保存 FPU 寄存器

        "stmdb r0!, {r4-r11}\n"         // 5. 保存 R4-R11 到任务栈

        // 6. 保存当前栈指针到当前 TCB
        "ldr r1, =current_task_ptr\n"
        "ldr r2, [r1]\n"
        "str r0, [r2]\n"                // tcb->stack_ptr = sp

        // 7. 调用调度器选择下一个任务
        "push {r1, r14}\n"
        "bl os_sched_select_next\n"
        "pop {r1, r14}\n"

        // 8. 从新任务 TCB 加载栈指针
        "ldr r2, [r1]\n"
        "ldr r0, [r2]\n"                // sp = new_tcb->stack_ptr

        // 9. 恢复 R4-R11
        "ldmia r0!, {r4-r11}\n"

        "tst r14, #0x10\n"              // 10. 检查 FPU
        "it eq\n"
        "vldmiaeq r0!, {s16-s31}\n"     // 11. 恢复 FPU 寄存器

        "msr psp, r0\n"                 // 12. 更新 PSP
        "cpsie i\n"                     // 13. 开中断
        "bx r14\n"                      // 14. 中断返回 (切换到新任务)
    );
}
```

**`__attribute__((naked))`** 的含义：

告诉编译器不要生成函数序言（push {lr}）和尾声（pop {pc}），因为 PendSV_Handler 的整个实现都在内联汇编中，编译器生成的序言/尾语会破坏寄存器状态。

**FPU 寄存器处理**：

Cortex-M3 没有 FPU，但为了兼容 Cortex-M4（有 FPU），代码中包含了 S16-S31 的保存/恢复逻辑。`tst r14, #0x10` 检查 EXC_RETURN 的 bit 4，如果为 0 表示使用了 FPU 扩展帧。

### 9.5 SysTick 处理

```c
void SysTick_Handler(void)
{
    extern void os_kernel_tick_increment(void);
    os_kernel_tick_increment();     // 递增 tick，处理延时，选择下一个任务
    os_port_yield();                // 触发 PendSV 进行上下文切换
}
```

**SysTick 配置**：

```c
void os_port_systick_init(uint32_t freq_hz)
{
    uint32_t reload = 72000000UL / freq_hz;  // 72MHz / 1000Hz = 72000
    SYSTICK->LOAD = reload - 1;
    SYSTICK->VAL  = 0;
    SYSTICK->CTRL = ENABLE | TICKINT | CLKSOURCE;

    // PendSV 设为最低优先级 (0xFF)
    SCB_SHPR3 |= (0xFFUL << 16);
    // SysTick 优先级略高于 PendSV
    SCB_SHPR3 |= (0xFFUL << 24);
}
```

### 9.6 第一次任务启动

```c
void os_port_start_first_task(void)
{
    os_tcb_t *tcb = os_task_get_current();

    __asm volatile(
        "msr psp, %0\n"         // 设置 PSP 为第一个任务的栈指针
        "mov r0, #2\n"          // CONTROL.SPSEL = 1 (使用 PSP)
        "msr control, r0\n"
        "isb\n"                 // 指令同步屏障
        "pop {r4-r11}\n"        // 恢复软件保存的寄存器
        "pop {r0-r3}\n"         // 恢复硬件保存的寄存器
        "pop {r12}\n"
        "pop {lr}\n"
        "pop {pc}\n"            // 跳转到任务入口函数
        :
        : "r"(tcb->stack_ptr)
    );
}
```

这个函数是裸机到多任务的"桥梁"。它手动从第一个任务的栈中弹出所有寄存器，最后 `pop {pc}` 跳转到任务入口函数，开始执行第一个任务。

---

## 10. 启动文件与链接脚本

### 10.1 启动文件 (startup_stm32f103.s)

启动文件是芯片上电后最先执行的代码，负责：

1. **设置初始栈指针**：`ldr sp, =_estack`
2. **复制 .data 段**：将 Flash 中的初始值复制到 RAM
3. **清零 .bss 段**：将未初始化的全局变量清零
4. **调用 main()**：跳转到 C 语言入口
5. **定义中断向量表**：包含所有中断处理函数的地址

**向量表结构**：

```
地址偏移 | 含义
0x00     | 初始栈指针 (_estack)
0x04     | Reset_Handler
0x08     | NMI_Handler
0x0C     | HardFault_Handler
...
0x3C     | SysTick_Handler
0x40     | WWDG_IRQHandler (外部中断开始)
...
```

所有中断处理函数默认指向 `Default_Handler`（死循环），使用 `.weak` 弱符号声明，允许在 C 代码中覆盖。

### 10.2 链接脚本 (stm32f103.ld)

定义了 STM32F103C8T6 的内存布局：

```
Flash: 0x08000000, 64KB  (代码 + 只读数据)
RAM:   0x20000000, 20KB  (数据 + BSS + 堆 + 栈)
```

**段布局**：

```
Flash (0x08000000):
  .isr_vector  — 中断向量表
  .text        — 程序代码
  .rodata      — 只读数据
  .data init   — 已初始化全局变量的初始值 (运行时复制到 RAM)

RAM (0x20000000):
  .data        — 已初始化全局变量
  .bss         — 未初始化全局变量 (清零)
  _user_heap   — newlib 堆 (512B)
  _user_stack  — MSP 栈 (1024B)
```

---

## 11. 上下文切换完整流程分析

### 11.1 主动让出 CPU (任务调用 OS_DELAY_MS)

```
任务 A 调用 OS_DELAY_MS(100)
        |
        v
os_task_delay(100)
  - 将 A 从就绪链表移除
  - A->delay_ticks = 100
  - A->state = BLOCKED
  - 将 A 加入 blocked_list
  - 调用 os_sched_yield()
        |
        v
os_sched_yield()
  - yield_pending = 1
  - os_port_yield()  →  SCB->ICSR |= PENDSVSET
        |
        v
PendSV 触发 (优先级最低，等当前 ISR 执行完)
        |
        v
PendSV_Handler:
  1. CPSID I (关中断)
  2. 保存 A 的 R4-R11 到 A 的栈
  3. A->stack_ptr = PSP
  4. os_sched_select_next()  →  选择最高优先级就绪任务 B
  5. PSP = B->stack_ptr
  6. 恢复 B 的 R4-R11
  7. CPSIE I (开中断)
  8. BX R14 (中断返回，CPU 开始执行 B)
```

### 11.2 时间片轮转 (SysTick 中断触发)

```
任务 A 正在运行 (优先级 4, 时间片 5 ticks)
        |
        v  (第 5 次 SysTick 中断)
SysTick_Handler:
  os_kernel_tick_increment()
    system_tick++
    os_task_tick()          // 处理延时任务
    os_sched_select_next()  // A 移到链表尾部, B 成为新 head
  os_port_yield()           // 触发 PendSV
        |
        v
PendSV_Handler:
  保存 A 的上下文
  切换到 B
  恢复 B 的上下文
  返回到 B
```

### 11.3 抢占 (高优先级任务就绪)

```
低优先级任务 A 正在运行
        |
        v  (某个 ISR 中唤醒了高优先级任务 B)
ISR 中:
  os_task_resume(B_handle)  // B 加入就绪链表
  os_sched_request_switch_from_isr()  // yield_pending = 1
        |
        v  (ISR 返回时)
SysTick_Handler 或 PendSV 触发
        |
        v
PendSV_Handler:
  保存 A 的上下文
  os_sched_select_next()  // B 优先级更高, 选择 B
  切换到 B
  恢复 B 的上下文
  返回到 B
```

---

## 12. API 参考手册

### 12.1 内核 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_kernel_init()` | 初始化内核所有子系统 | void |
| `os_kernel_start()` | 启动调度器 (永不返回) | void |
| `os_kernel_get_tick()` | 获取当前系统 tick | `os_tick_t` |
| `os_kernel_get_version()` | 获取版本字符串 | `const char*` |

### 12.2 任务 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_task_create()` | 创建任务 (立即进入 READY 状态) | `os_status_t` |
| `os_task_create_suspended()` | 创建任务 (进入 SUSPENDED 状态) | `os_status_t` |
| `os_task_delete()` | 删除任务 (传 NULL 删除自身，支持延迟释放) | `os_status_t` |
| `os_task_suspend()` | 挂起任务 | `os_status_t` |
| `os_task_resume()` | 恢复任务 | `os_status_t` |
| `os_task_delay()` | 延时 (tick) | `os_status_t` |
| `os_task_set_priority()` | 修改优先级 | `os_status_t` |
| `os_task_get_name()` | 获取任务名 | `const char*` |
| `os_task_get_state()` | 获取任务状态 | `os_task_state_t` |
| `os_task_get_priority()` | 获取任务优先级 | `os_prio_t` |
| `os_task_get_count()` | 获取当前任务总数 | `uint32_t` |
| `os_task_get_stack_high_water()` | 获取栈使用峰值 | `uint32_t` |

### 12.3 堆 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_heap_init()` | 初始化堆 | void |
| `os_heap_alloc(size)` | 分配内存 | `void*` |
| `os_heap_calloc(count, size)` | 分配并清零 | `void*` |
| `os_heap_free(ptr)` | 释放内存 | void |
| `os_heap_realloc(ptr, new_size)` | 重新分配 | `void*` |
| `os_heap_get_free_size()` | 获取空闲大小 | `uint32_t` |
| `os_heap_get_largest_free_block()` | 获取最大空闲块 | `uint32_t` |
| `os_heap_get_min_free_size()` | 获取历史最小空闲 | `uint32_t` |

### 12.4 便捷宏

| 宏 | 展开为 | 说明 |
|----|--------|------|
| `OS_DELAY(ticks)` | `os_task_delay(ticks)` | 延时 N tick |
| `OS_DELAY_MS(ms)` | `os_task_delay(ms * 1000 / HZ)` | 延时 N 毫秒 |
| `OS_DELAY_SEC(sec)` | `os_task_delay(sec * HZ)` | 延时 N 秒 |
| `OS_GET_TICK()` | `os_kernel_get_tick()` | 获取当前 tick |
| `OS_ENTER_CRITICAL()` | `os_sched_enter_critical()` | 进入临界区 |
| `OS_EXIT_CRITICAL()` | `os_sched_exit_critical()` | 退出临界区 |
| `OS_YIELD()` | `os_sched_yield()` | 主动让出 CPU |
| `OS_TASK_DELETE_SELF()` | `os_task_delete(NULL)` | 删除当前任务 |
| `OS_TASK_GET_STATE(h)` | `os_task_get_state(h)` | 查询任务状态 |
| `OS_TASK_GET_PRIO(h)` | `os_task_get_priority(h)` | 查询任务优先级 |
| `OS_TASK_GET_COUNT()` | `os_task_get_count()` | 查询任务总数 |

### 12.5 消息队列 API (v0.2.0)

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_queue_create(queue, item_size, max_items)` | 创建队列 | `os_status_t` |
| `os_queue_delete(queue)` | 删除队列 | `os_status_t` |
| `os_queue_send(queue, item, timeout)` | 发送 (阻塞) | `os_status_t` |
| `os_queue_send_from_isr(queue, item)` | 发送 (ISR 安全) | `os_status_t` |
| `os_queue_receive(queue, item, timeout)` | 接收 (阻塞) | `os_status_t` |
| `os_queue_receive_from_isr(queue, item)` | 接收 (ISR 安全) | `os_status_t` |
| `os_queue_peek(queue, item)` | 查看不取出 | `os_status_t` |
| `os_queue_get_count(queue)` | 获取元素数 | `uint32_t` |
| `os_queue_get_spaces(queue)` | 获取剩余空间 | `uint32_t` |
| `os_queue_is_empty(queue)` | 是否为空 | `bool` |
| `os_queue_is_full(queue)` | 是否已满 | `bool` |

### 12.6 信号量 API (v0.2.0)

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_sem_create_binary(sem)` | 创建二值信号量 | `os_status_t` |
| `os_sem_create_counting(sem, max, init)` | 创建计数信号量 | `os_status_t` |
| `os_sem_delete(sem)` | 删除信号量 | `os_status_t` |
| `os_sem_take(sem, timeout)` | 获取 (P 操作) | `os_status_t` |
| `os_sem_give(sem)` | 释放 (V 操作) | `os_status_t` |
| `os_sem_give_from_isr(sem)` | 释放 (ISR 安全) | `os_status_t` |
| `os_sem_get_count(sem)` | 获取当前计数 | `uint32_t` |

### 12.7 互斥锁 API (v0.2.0)

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_mutex_create(mutex)` | 创建互斥锁 | `os_status_t` |
| `os_mutex_delete(mutex)` | 删除互斥锁 | `os_status_t` |
| `os_mutex_lock(mutex, timeout)` | 加锁 (支持递归) | `os_status_t` |
| `os_mutex_unlock(mutex)` | 解锁 | `os_status_t` |
| `os_mutex_get_owner(mutex)` | 查询持有者 | `os_tcb_t*` |

### 12.8 软件定时器 API (v0.2.0)

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_timer_create(timer, name, period, type, cb)` | 创建定时器 | `os_status_t` |
| `os_timer_delete(timer, timeout)` | 删除定时器 | `os_status_t` |
| `os_timer_start(timer, timeout)` | 启动定时器 | `os_status_t` |
| `os_timer_stop(timer, timeout)` | 停止定时器 | `os_status_t` |
| `os_timer_reset(timer, timeout)` | 重置定时器 | `os_status_t` |
| `os_timer_change_period(timer, period, timeout)` | 修改周期 | `os_status_t` |
| `os_timer_is_active(timer)` | 是否激活 | `bool` |

### 12.9 事件标志组 API (v0.2.0)

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_eventgroup_create(eg)` | 创建事件组 | `os_status_t` |
| `os_eventgroup_delete(eg)` | 删除事件组 | `os_status_t` |
| `os_eventgroup_set_bits(eg, bits)` | 设置事件位 | `os_status_t` |
| `os_eventgroup_set_bits_from_isr(eg, bits)` | 设置 (ISR 安全) | `os_status_t` |
| `os_eventgroup_clear_bits(eg, bits)` | 清除事件位 | `os_status_t` |
| `os_eventgroup_wait_bits(eg, bits, opts, timeout)` | 等待事件位 | `uint32_t` |
| `os_eventgroup_get_bits(eg)` | 获取当前位 | `uint32_t` |

### 12.10 新增便捷宏 (v0.2.0)

| 宏 | 展开为 | 说明 |
|----|--------|------|
| `OS_QUEUE_SEND(q, item, t)` | `os_queue_send(q, item, t)` | 队列发送 |
| `OS_QUEUE_RECEIVE(q, item, t)` | `os_queue_receive(q, item, t)` | 队列接收 |
| `OS_SEM_GIVE(s)` | `os_sem_give(s)` | 信号量释放 |
| `OS_SEM_TAKE(s, t)` | `os_sem_take(s, t)` | 信号量获取 |
| `OS_MUTEX_LOCK(m, t)` | `os_mutex_lock(m, t)` | 互斥锁加锁 |
| `OS_MUTEX_UNLOCK(m)` | `os_mutex_unlock(m)` | 互斥锁解锁 |

---

## 13. 构建系统

### 13.1 工具链

| 工具 | 用途 |
|------|------|
| `arm-none-eabi-gcc` | C 编译器 |
| `arm-none-eabi-as` | 汇编器 |
| `arm-none-eabi-ld` | 链接器 |
| `arm-none-eabi-objcopy` | 格式转换 (ELF → HEX/BIN) |
| `arm-none-eabi-objdump` | 反汇编 |
| `arm-none-eabi-size` | 查看段大小 |

### 13.2 编译选项

```
CFLAGS = -mcpu=cortex-m3 -mthumb    # Cortex-M3 Thumb 指令集
         -Wall -Wextra -Werror       # 严格警告
         -fdata-sections             # 每个数据放入独立段 (配合 --gc-sections)
         -ffunction-sections         # 每个函数放入独立段
         -g -O2                      # 调试信息 + 优化级别 2
         -std=c11                    # C11 标准
         -DSTM32F103xB               # 芯片型号宏
```

### 13.3 链接选项

```
LDFLAGS = -T port/stm32f103.ld      # 链接脚本
          -Wl,--gc-sections           # 删除未使用的段
          -Wl,-Map=build/minios.map   # 生成链接映射
          --specs=nosys.specs         # 无系统调用
          --specs=nano.specs          # 使用 newlib-nano (精简版 C 库)
          -lc -lm -lnosys             # 链接 C 库和数学库
```

### 13.4 Make 目标

| 目标 | 说明 |
|------|------|
| `make all` | 编译生成 .elf, .hex, .bin |
| `make clean` | 清除构建产物 |
| `make size` | 显示段大小信息 |
| `make info` | 显示项目信息 |
| `make flash` | 通过 OpenOCD + ST-Link 烧录 |

### 13.5 输出文件

| 文件 | 说明 |
|------|------|
| `build/minios.elf` | ELF 可执行文件 (带调试信息) |
| `build/minios.hex` | Intel HEX (烧录用) |
| `build/minios.bin` | 纯二进制 (烧录用) |
| `build/minios.map` | 链接映射 (分析符号地址) |
| `build/minios.lst` | 反汇编列表 |

---

## 14. 内存布局与资源预算

### 14.1 STM32F103C8T6 资源

| 资源 | 大小 |
|------|------|
| Flash | 64 KB |
| RAM | 20 KB |
| CPU | Cortex-M3, 72 MHz |

### 14.2 RAM 使用分析 (v0.2.0)

```
RAM (0x20000000, 20KB):
+---------------------------+  0x20005000
| MSP 栈 (中断栈)           |  1024 B
+---------------------------+
| newlib 堆                 |  256 B
+---------------------------+
| .bss 段                   |
|   uc_heap[14KB + 16B]     |  ~14352 B
|   定时器服务栈             |  256 B
|   其他全局变量             |  ~300 B
+---------------------------+
| .data 段                  |  ~0 B
+---------------------------+  0x20000000

总 RAM 使用: ~18.5 KB / 20 KB (92.5%)
```

### 14.3 任务栈分配 (v0.2.0)

| 任务 | 栈大小 | 用途 |
|------|--------|------|
| Task1 (PROD) | 512 B | 队列生产者 |
| Task2 (CONS) | 512 B | 队列消费者 |
| Task3 (EVT) | 512 B | 事件等待者 |
| Task4 (MON) | 512 B | 系统监控 |
| IDLE | 256 B | 空闲任务 |
| TMR | 256 B | 定时器服务任务 |
| **总计** | **2560 B** | |

### 14.4 Flash 使用分析 (v0.2.0)

```
Flash (0x08000000, 64KB):
+---------------------------+
| .isr_vector               |  ~240 B (60 个中断向量)
| .text                     |  ~5.3 KB (代码)
| .rodata                   |  ~200 B (常量)
| .data init values         |  ~0 B
+---------------------------+

总 Flash 使用: ~5.7 KB / 64 KB (8.9%)
```

---

## 15. 已知限制与注意事项

### 15.1 功能限制

1. ~~**无同步原语**：没有互斥锁、信号量、事件标志组。任务间无法安全共享数据。~~ ✅ 已实现 (v0.2.0)
2. ~~**无消息传递**：没有消息队列、邮箱。任务间无法通信。~~ ✅ 已实现队列 (v0.2.0)
3. ~~**无软件定时器**：只能使用 SysTick 作为系统时钟源。~~ ✅ 已实现 (v0.2.0)
4. **不支持动态优先级**：优先级在创建时确定，运行时只能通过 `os_task_set_priority()` 手动修改。
5. ~~**无邮箱 (Mailbox)**：单元素消息队列，可基于队列实现。~~ ✅ 已实现 (v0.5.0)
6. ~~**无 Tickless Idle**：空闲时仍保持 SysTick 运行。~~ ✅ 已实现 (v0.5.0)

### 15.2 实现注意事项

1. **PendSV 使用内联汇编**：正式项目建议使用独立的 `.s` 文件，避免编译器优化带来的问题。
2. **`current_task_ptr` 是全局变量**：在 `port.c` 中定义，通过 `extern` 在其他文件中访问。汇编代码直接引用这个符号。
3. **堆对齐**：堆内存池使用 `__attribute__((aligned(8)))` 确保 8 字节对齐。
4. **栈对齐**：ARM AAPCS 要求 8 字节栈对齐，`os_port_stack_init()` 中已处理。
5. **SysTick 假设 72MHz**：`os_port_systick_init()` 中硬编码了 72MHz 时钟，移植到其他频率的芯片需要修改。

### 15.3 潜在问题

1. **堆碎片**：长时间运行可能导致堆碎片化，虽然 Heap-4 的合并机制可以缓解，但无法完全消除。
2. **栈溢出检测的局限**：当前的 canary 检测只能发现栈底被覆盖的情况，无法检测数组越界等其他类型的内存错误。
3. **临界区粒度**：当前使用全局关中断 (`CPSID I`)，在高频率中断场景下可能导致中断延迟增大。

---

## 16. 后续扩展方向

### 第一优先级：核心同步机制 ✅ 已完成 (v0.2.0)

| 功能 | 说明 | 状态 |
|------|------|------|
| 互斥锁 (Mutex) | 任务间互斥访问共享资源，支持优先级继承 | ✅ |
| 信号量 (Semaphore) | 二值/计数信号量，资源计数和任务同步 | ✅ |
| 消息队列 (Queue) | 任务间消息传递，阻塞收发，ISR 安全 | ✅ |

### 第二优先级：定时与中断

| 功能 | 说明 | 状态 |
|------|------|------|
| 软件定时器 | 基于 tick 的回调定时器，单次/自动重载 | ✅ |
| 事件标志组 | 多条件组合等待 (等待任意/全部位) | ✅ |
| 中断优先级管理 | NVIC 优先级分组配置 | ✅ |
| 中断嵌套支持 | 允许高优先级中断抢占低优先级 ISR | ✅ |

### 第三优先级：调试与优化

| 功能 | 说明 | 状态 |
|------|------|------|
| ~~栈溢出检测~~ | ~~在 tick 中检查栈边界~~ | ✅ |
| 运行时统计 | 每个任务的 CPU 占用率 | ✅ |
| 任务状态查看 | 导出所有任务的状态信息 | ✅ |
| Trace 支持 | 记录内核事件用于离线分析 | ✅ |

### 第四优先级：高级特性

| 功能 | 说明 | 状态 |
|------|------|------|
| ~~Tickless Idle~~ | ~~空闲时停止 SysTick，进一步降低功耗~~ | ✅ |
| 内存池 | 固定大小块分配器，O(1) 分配无碎片 | ✅ |
| ~~邮箱 (Mailbox)~~ | ~~基于队列的单元素消息传递~~ | ✅ |
| 移植到 Cortex-M0/M4/M7 | 扩展硬件支持 | ✅ |

---

## 17. 消息队列 (queue.c) — v0.2.0 新增

### 17.1 设计目标

实现任务间消息传递机制，支持阻塞发送/接收和 ISR 安全变体。

### 17.2 数据结构

```c
typedef struct os_queue {
    uint8_t     *buffer;            // 环形缓冲区 (堆分配)
    uint32_t    item_size;          // 每个元素大小 (字节)
    uint32_t    max_items;          // 最大元素数
    uint32_t    count;              // 当前元素数
    uint32_t    head;               // 下次读取位置 (字节索引)
    uint32_t    tail;               // 下次写入位置 (字节索引)
    uint32_t    buf_size;           // 缓冲区总大小
    os_tcb_t    *send_wait_list;    // 发送等待链表 (优先级排序)
    os_tcb_t    *recv_wait_list;    // 接收等待链表 (优先级排序)
} os_queue_t;
```

### 17.3 环形缓冲区

缓冲区为连续的字节数组，`head` 和 `tail` 为字节索引。写入时 `tail = (tail + item_size) % buf_size`，读取时同理。当 `count == max_items` 时队列满，`count == 0` 时队列空。

### 17.4 阻塞机制

- **发送阻塞**: 队列满时，任务加入 `send_wait_list`，设置 `blocked_on = queue`、`blocked_reason = OS_BLOCKED_ON_QUEUE_SEND`
- **接收阻塞**: 队列空时，任务加入 `recv_wait_list`，设置 `blocked_reason = OS_BLOCKED_ON_QUEUE_RECV`
- **超时处理**: `os_task_tick()` 递减 `delay_ticks`，到期时设置 `timed_out = 1` 并移回就绪链表

### 17.5 唤醒策略

发送成功后唤醒 `recv_wait_list` 中最高优先级任务；接收成功后唤醒 `send_wait_list` 中最高优先级任务。如果被唤醒任务优先级高于当前任务，触发调度。

### 17.6 ISR 变体

`os_queue_send_from_isr()` 和 `os_queue_receive_from_isr()` 使用 `os_port_enter_critical()` 直接操作，不调用 `os_sched_enter_critical()`。唤醒任务后调用 `os_sched_request_switch_from_isr()` 请求延迟切换。

---

## 18. 信号量 (semaphore.c) — v0.2.0 新增

### 18.1 设计目标

实现二值信号量 (任务同步) 和计数信号量 (资源计数)。

### 18.2 数据结构

```c
typedef struct os_sem {
    uint32_t    count;          // 当前计数
    uint32_t    max_count;      // 最大计数 (1 = 二值)
    os_tcb_t    *wait_list;     // 等待链表 (优先级排序)
} os_sem_t;
```

### 18.3 直接传递模式

`os_sem_give()` 的关键行为：如果有等待者，不增加计数，直接将最高优先级等待者移入就绪链表。这避免了计数超过实际可用资源的问题。

### 18.4 API

| 函数 | 说明 |
|------|------|
| `os_sem_create_binary(sem)` | 创建二值信号量 (初始值 0) |
| `os_sem_create_counting(sem, max, init)` | 创建计数信号量 |
| `os_sem_delete(sem)` | 删除信号量 (唤醒所有等待者) |
| `os_sem_take(sem, timeout)` | 获取 (P 操作) |
| `os_sem_give(sem)` | 释放 (V 操作) |
| `os_sem_give_from_isr(sem)` | ISR 安全释放 |

---

## 19. 互斥锁 (mutex.c) — v0.2.0 新增

### 19.1 设计目标

保护共享资源，通过优先级继承防止优先级反转。

### 19.2 数据结构

```c
typedef struct os_mutex {
    os_tcb_t    *owner;         // 持有者 (NULL = 空闲)
    uint32_t    lock_count;     // 锁定深度 (支持递归)
    os_prio_t   original_prio;  // 持有者原始优先级
    os_tcb_t    *wait_list;     // 等待链表 (优先级排序)
} os_mutex_t;
```

### 19.3 优先级继承

**问题**: 任务 L (低优先级) 持有 mutex，任务 H (高优先级) 等待 mutex，任务 M (中优先级) 抢占 L。此时 H 被 M 间接阻塞 (优先级反转)。

**解决**: 当 H 等待 mutex 时，临时将 L 的优先级提升到 H 的级别，防止 M 抢占 L。

**时机**:
- **提升**: `os_mutex_lock()` 中，如果当前任务优先级高于持有者
- **恢复**: `os_mutex_unlock()` 中，锁计数降为 0 时恢复原始优先级

### 19.4 递归锁定

同一任务可以多次 `os_mutex_lock()`，每次 `lock_count++`。需要对应次数的 `os_mutex_unlock()` 才能完全释放。不同任务尝试加锁会阻塞。

### 19.5 所有权转移

`os_mutex_unlock()` 完全释放后，如果有等待者，自动将所有权转移给最高优先级等待者 (设置 `owner`、`lock_count`、`original_prio`)。

---

## 20. 软件定时器 (timer.c) — v0.2.0 新增

### 20.1 设计目标

基于系统 tick 的回调定时器，支持单次和自动重载模式。

### 20.2 架构

```
SysTick ISR
  └─> os_timer_tick()        // 递减定时器，到期移入 expired_list
        └─> os_sem_give()    // 唤醒定时器服务任务

定时器服务任务 (TMR, 低优先级)
  └─> os_sem_take()          // 等待信号量
  └─> 遍历 expired_list      // 调用每个到期定时器的回调
```

### 20.3 活跃定时器链表

定时器按 `remaining` 升序排列在 `active_timer_list` 中。每个定时器的 `remaining` 是相对于前一个定时器的差值 (delta list)，这样只需递减链表头的计数器。

### 20.4 数据结构

```c
typedef struct os_timer {
    os_timer_callback_t callback;   // 回调函数
    os_tick_t           period;     // 周期 (tick)
    os_tick_t           remaining;  // 相对剩余 tick
    os_timer_type_t     type;       // ONE_SHOT / AUTO_RELOAD
    bool                active;     // 是否激活
    struct os_timer     *next;      // 链表指针
} os_timer_t;
```

### 20.5 回调执行上下文

回调在定时器服务任务中执行 (非 ISR 上下文)，因此可以调用阻塞 API (如 `os_queue_send()`)。

---

## 21. 事件标志组 (eventgroup.c) — v0.2.0 新增

### 21.1 设计目标

32 位事件标志，支持等待任意位或全部位满足。

### 21.2 数据结构

```c
typedef struct os_eventgroup {
    uint32_t    bits;           // 当前事件位
    os_tcb_t    *wait_list;     // 等待链表
} os_eventgroup_t;
```

### 21.3 等待选项

| 选项 | 说明 |
|------|------|
| `OS_EVENT_WAIT_ANY` | 任意请求位满足即唤醒 (默认) |
| `OS_EVENT_WAIT_ALL` | 所有请求位满足才唤醒 |
| `OS_EVENT_CLEAR_ON_EXIT` | 唤醒时清除匹配位 |

### 21.4 TCB 扩展字段

事件等待需要在 TCB 中存储等待模式：

```c
uint32_t event_wait_bits;     // 等待的位
uint32_t event_wait_options;  // 等待选项
uint32_t event_return_bits;   // 唤醒时的 bits 值
```

### 21.5 set_bits 唤醒逻辑

`os_eventgroup_set_bits()` 设置位后，遍历 `wait_list` 检查每个任务的等待条件。满足条件的任务被移入就绪链表，如果 `CLEAR_ON_EXIT` 则清除对应位。多个任务可能在一次 set_bits 中被唤醒。

---

## 22. 内存池分配器 (mempool.c) — v0.3.0 新增

### 22.1 设计目标

Heap-4 采用首次适配算法，适合变长分配，但在高频分配/释放相同大小对象的场景下存在两个问题：

1. **碎片化**: 反复 alloc/free 不同生命周期的同大小块会产生外部碎片
2. **性能**: O(n) 遍历空闲链表寻找合适块，不适合时间敏感路径

内存池 (Memory Pool) 解决这两个问题：将一块连续内存划分为等大的块，用空闲链表串联。分配和释放均为 O(1)，且零碎片。

### 22.2 适用场景

| 场景 | 说明 |
|------|------|
| TCB 分配 | 任务创建/删除频繁时，用固定大小的 TCB 池替代 heap_alloc |
| 网络数据包 | 网络栈中固定大小的 packet buffer |
| IPC 消息 | 固定大小的消息结构体频繁收发 |
| 中断上下文分配 | 需要从 ISR 中快速获取内存时 |

### 22.3 内存布局

```
用户提供的缓冲区:
+-----------------------------------------------------------+
| Block 0 | Block 1 | Block 2 | ... | Block N-1            |
+-----------------------------------------------------------+

每个 Block 的布局:
  已分配时: [用户数据 (block_size 字节)]
  空闲时:   [next 指针 | 填充...]  (前 sizeof(void*) 字节存放链表指针)
```

**关键设计**: 空闲链表的指针直接嵌入在块的数据区中（无需额外 header），已分配的块对用户完全透明，无任何元数据开销。这是内存池高效的根本原因。

### 22.4 空闲链表结构

空闲块构成一个单向链表 (栈式 LIFO)：

```
free_list --> [Block 3] --> [Block 7] --> [Block 1] --> NULL
               (head)                                (tail)
```

- **分配**: 取出链表头 (`pop head`)，O(1)
- **释放**: 插入链表头 (`push head`)，O(1)

### 22.5 数据结构

```c
typedef struct os_mempool {
    uint8_t     *pool_start;        // 池起始地址
    uint8_t     *pool_end;          // 池结束地址 (不含)
    uint32_t    block_size;         // 用户可见的块大小
    uint32_t    total_blocks;       // 总块数
    uint32_t    free_count;         // 当前空闲块数
    uint32_t    min_free_count;     // 空闲块历史最低值 (高水位标记)
    void        *free_list;         // 空闲链表头
} os_mempool_t;
```

### 22.6 创建流程

`os_mempool_create()` 的工作：

1. 对齐缓冲区起始地址到指针边界
2. 计算实际块步长 = `ALIGN_UP(block_size)`，确保每块至少容纳一个指针
3. 计算可容纳的块数 = `可用空间 / 步长`
4. 遍历所有块，将每个块的前 `sizeof(void*)` 字节指向下一个块，构建空闲链表
5. 最后一个块的 next 指向 NULL

### 22.7 分配流程

```
os_mempool_alloc(pool):
    if pool->free_count == 0:
        return NULL                        // 池已耗尽

    block = pool->free_list                // 取出链表头
    pool->free_list = *(void**)block       // 头指针后移
    pool->free_count--

    return block                           // 返回给用户
```

### 22.8 释放流程

```
os_mempool_free(pool, ptr):
    // 边界检查: ptr 必须在 [pool_start, pool_end) 范围内
    // 对齐检查: ptr 必须在块的边界上

    *(void**)ptr = pool->free_list         // 新块指向原链表头
    pool->free_list = ptr                  // 新块成为链表头
    pool->free_count++
```

### 22.9 与 Heap-4 的对比

| 特性 | Heap-4 | Memory Pool |
|------|--------|-------------|
| 分配大小 | 任意 | 固定 |
| 分配复杂度 | O(n) | O(1) |
| 释放复杂度 | O(1) (摊销) | O(1) |
| 碎片化 | 可能产生外部碎片 | 零碎片 |
| 元数据开销 | 每块 12 字节 header | 0 (空闲时复用数据区) |
| 适用场景 | 通用动态分配 | 固定大小对象高频分配 |

### 22.10 API 参考

```c
/* 创建内存池 */
os_status_t os_mempool_create(os_mempool_t *pool, void *buf,
                              uint32_t buf_size, uint32_t block_size);

/* 分配一个块 (O(1)) */
void* os_mempool_alloc(os_mempool_t *pool);

/* 释放一个块 (O(1)) */
os_status_t os_mempool_free(os_mempool_t *pool, void *ptr);

/* 查询 API */
uint32_t os_mempool_get_free_count(os_mempool_t *pool);
uint32_t os_mempool_get_min_free_count(os_mempool_t *pool);
uint32_t os_mempool_get_total_count(os_mempool_t *pool);
bool os_mempool_owns(os_mempool_t *pool, void *ptr);
```

### 22.11 使用示例

```c
/* 定义一个 16 块、每块 32 字节的内存池 */
#define BLOCK_SIZE   32
#define BLOCK_COUNT  16

static os_mempool_t my_pool;
static uint8_t pool_buf[BLOCK_SIZE * BLOCK_COUNT];

void pool_demo(void)
{
    os_mempool_create(&my_pool, pool_buf, sizeof(pool_buf), BLOCK_SIZE);

    /* 分配 */
    void *p1 = os_mempool_alloc(&my_pool);  // 获取一个块
    void *p2 = os_mempool_alloc(&my_pool);  // 获取另一个块

    /* 使用 ... */

    /* 释放 */
    os_mempool_free(&my_pool, p1);
    os_mempool_free(&my_pool, p2);

    /* 查询 */
    uint32_t free = os_mempool_get_free_count(&my_pool);  // 16
    uint32_t min  = os_mempool_get_min_free_count(&my_pool); // 14
}
```

### 22.12 配置宏

```c
/* config/os_config.h */
#define OS_CONFIG_USE_MEMPOOL   1   // 启用内存池模块 (设为 0 可裁剪)
```

---

## 23. 邮箱 (mailbox.c) — v0.5.0 新增

### 23.1 设计目标

邮箱是容量为 1 的消息队列，用于单值信号传递。相比通用队列，邮箱接口更简洁，语义更明确。

### 23.2 实现方式

邮箱内部封装一个 `os_queue_t`，固定 `max_items = 1`。所有操作委托给队列 API。

```c
typedef struct os_mailbox {
    os_queue_t  queue;              /* Underlying queue (capacity 1) */
} os_mailbox_t;
```

### 23.3 与队列的区别

| 特性 | 队列 (Queue) | 邮箱 (Mailbox) |
|------|-------------|---------------|
| 容量 | 可配置 (N) | 固定 1 |
| 语义 | 多元素缓冲 | 单值信号 |
| 典型用途 | 生产者-消费者流水线 | 告警、状态通知 |
| API 复杂度 | 相同 | 相同但语义更清晰 |

### 23.4 API 参考

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `os_mailbox_create(mb, item_size)` | 创建邮箱 | `os_status_t` |
| `os_mailbox_delete(mb)` | 删除邮箱 | `os_status_t` |
| `os_mailbox_send(mb, item, timeout)` | 发送 (满时阻塞) | `os_status_t` |
| `os_mailbox_receive(mb, item, timeout)` | 接收 (空时阻塞) | `os_status_t` |
| `os_mailbox_send_from_isr(mb, item)` | 发送 (ISR 安全) | `os_status_t` |
| `os_mailbox_receive_from_isr(mb, item)` | 接收 (ISR 安全) | `os_status_t` |
| `os_mailbox_is_empty(mb)` | 是否为空 | `bool` |
| `os_mailbox_is_full(mb)` | 是否已满 | `bool` |

### 23.5 使用示例

```c
static os_mailbox_t alarm_mb;

void alarm_task(void *param)
{
    uint32_t code;
    os_mailbox_create(&alarm_mb, sizeof(uint32_t));

    while (1) {
        if (os_mailbox_receive(&alarm_mb, &code, OS_WAIT_FOREVER) == OS_OK) {
            handle_alarm(code);
        }
    }
}

void isr_handler(void)
{
    uint32_t code = 0xDEAD;
    os_mailbox_send_from_isr(&alarm_mb, &code);
}
```

### 23.6 配置宏

```c
/* config/os_config.h */
#define OS_CONFIG_USE_MAILBOX   1   // 启用邮箱模块 (设为 0 可裁剪)
```

---

## 24. Tickless Idle (tickless.c) — v0.5.0 新增

### 24.1 设计目标

在空闲期间停止 SysTick 定时器，降低 CPU 功耗。适用于电池供电或低功耗应用场景。

### 24.2 工作原理

```
正常模式:
  SysTick 每 1ms 中断 → 唤醒 CPU → 处理 → WFI → SysTick 中断 → ...
  即使没有任务需要运行，CPU 仍被每 1ms 唤醒一次。

Tickless 模式:
  空闲任务发现下一个唤醒在 N ms 后
    → 停止 SysTick
    → 配置一次性唤醒定时器 (N ms)
    → WFI (CPU 深度睡眠)
    → 定时器到期唤醒
    → 计算经过的 tick 数
    → 重启 SysTick
    → 补偿丢失的 tick
```

### 24.3 实现细节

**进入 Tickless**:
1. 读取当前 SysTick VAL（当前 tick 中剩余的 SysTick 周期数）
2. 停止周期性 SysTick
3. 计算总睡眠周期数 = `remaining + (expected_ticks - 1) * reload`
4. 配置 SysTick 为一次性定时器（无中断，轮询 COUNTFLAG）
5. 执行 WFI

**退出 Tickless**:
1. 读取 SysTick VAL 计算实际经过的周期数
2. 转换为系统 tick 数
3. 重启周期性 SysTick
4. 返回经过的 tick 数

### 24.4 API 参考

| 函数 | 说明 |
|------|------|
| `os_tickless_init()` | 初始化 (os_kernel_init 自动调用) |
| `os_tickless_idle_enter(ticks)` | 进入 tickless 模式 |
| `os_tickless_idle_exit()` | 退出，返回经过的 tick 数 |
| `os_tickless_is_active()` | 查询是否在 tickless 模式 |

### 24.5 配置宏

```c
/* config/os_config.h */
#define OS_CONFIG_USE_TICKLESS_IDLE     0   // 默认关闭
#define OS_TICKLESS_MIN_IDLE_TICKS      5   // 最小空闲 tick 才进入
```

### 24.6 限制

- SysTick 是 24 位定时器，最大睡眠时间约 16 秒 (72MHz)
- 唤醒后有微小的 tick 精度损失（几个 SysTick 周期）
- 需要配合实际硬件测试功耗降低效果

---

## 25. 移植指南 (Cortex-M0/M4/M7) — v0.6.0 新增

### 25.1 架构差异总览

| 特性 | Cortex-M3 | Cortex-M0 | Cortex-M4 | Cortex-M7 |
|------|----------|-----------|-----------|-----------|
| 指令集 | Thumb-2 | Thumb | Thumb-2 | Thumb-2 |
| FPU | 无 | 无 | 单精度 SP | 双精度 DP |
| BASEPRI | ✅ | ❌ | ✅ | ✅ |
| NVIC 优先级位 | 4 | 2 | 4 | 4 |
| 代表芯片 | STM32F103 | STM32F030 | STM32F407 | STM32F746 |
| 系统时钟 | 72 MHz | 48 MHz | 168 MHz | 216 MHz |

### 25.2 各 Port 层关键差异

#### Cortex-M0 (`port/cortex_m0/`)

**无 BASEPRI**: M0 只有 PRIMASK 寄存器，无法实现基于 BASEPRI 的中断嵌套。`port_m0.c` 中只有 PRIMASK 版本的临界区代码，且通过 `#error` 阻止 `OS_CONFIG_USE_INTERRUPT_NESTING=1` 的编译。

**PendSV_Handler**: M0 不支持 `stmdb` / `ldmia` 后递增的语法，需要用 `sub` + `stmia` 手动模拟。R8-R11 需要通过 R4-R7 中转保存。

**NVIC 差异**: M0 只有 1 个 ISER/ICER 寄存器（支持 32 个中断），IP 寄存器只有 8 个。

#### Cortex-M4 (`port/cortex_m4/`)

**FPU 上下文**: PendSV_Handler 在保存/恢复 R4-R11 前后，通过 `tst r14, #0x10` 检查 EXC_RETURN 的 bit 4。如果为 0，表示任务使用了 FPU，需要额外保存/恢复 S16-S31（16 个 32 位寄存器 = 64 字节）。

**FPU 使能**: `prv_fpu_enable()` 在 `os_kernel_init()` 中调用，设置 SCB.CPACR 的 CP10/CP11 为 full access。

#### Cortex-M7 (`port/cortex_m7/`)

**双精度 FPU**: M7 的 FPU 支持双精度浮点，D16-D31 是 16 个 64 位寄存器（32 个 32 位字）。PendSV 使用 `vstmdbeq r0!, {d16-d31}` / `vldmiaeq r0!, {d16-d31}` 保存/恢复，占用 32 word 栈空间（vs M4 的 16 word）。

### 25.3 如何适配新芯片

要将 MiniRTOS 移植到一个新的 Cortex-M 芯片，需要修改以下文件：

1. **`port.c`**: 修改 `os_port_systick_init()` 中的时钟频率常量
2. **`startup_*.s`**: 修改 `.cpu` 和 `.fpu` 指令、向量表中的中断列表
3. **`*.ld`**: 修改 Flash/RAM 地址和大小
4. **`os_config.h`**: 修改 `OS_CONFIG_NVIC_PRIO_BITS` 和 `OS_CONFIG_USE_INTERRUPT_NESTING`

**步骤**:
1. 复制最接近的 port 目录（如 `port/cortex_m4/`）
2. 查阅芯片数据手册，确认时钟频率、Flash/RAM 大小、中断列表
3. 修改上述 4 个文件
4. 更新 Makefile 中的 `C_SOURCES`、`ASM_SOURCES`、`LDSCRIPT`、`CPU`、`FPU` 变量

### 25.4 目录结构

```
port/
├── port.c / port.h                  # Cortex-M3 (STM32F103, 72MHz)
├── startup_stm32f103.s
├── stm32f103.ld
├── cortex_m0/
│   ├── port_m0.c                    # Cortex-M0 (STM32F030, 48MHz, 无 FPU/BASEPRI)
│   ├── startup_stm32f030.s
│   └── stm32f030.ld
├── cortex_m4/
│   ├── port_m4.c                    # Cortex-M4 (STM32F407, 168MHz, 单精度 FPU)
│   ├── startup_stm32f407.s
│   └── stm32f407.ld
└── cortex_m7/
    ├── port_m7.c                    # Cortex-M7 (STM32F746, 216MHz, 双精度 FPU)
    ├── startup_stm32f746.s
    └── stm32f746.ld
```

---

## 26. TCB 扩展 — v0.2.0 新增

### 25.1 架构差异总览

| 特性 | Cortex-M3 | Cortex-M0 | Cortex-M4 | Cortex-M7 |
|------|----------|-----------|-----------|-----------|
| 指令集 | Thumb-2 | Thumb | Thumb-2 | Thumb-2 |
| FPU | 无 | 无 | 单精度 SP | 双精度 DP |
| BASEPRI | ✅ | ❌ | ✅ | ✅ |
| NVIC 优先级位 | 4 | 2 | 4 | 4 |
| 代表芯片 | STM32F103 | STM32F030 | STM32F407 | STM32F746 |
| 系统时钟 | 72 MHz | 48 MHz | 168 MHz | 216 MHz |

### 25.2 各 Port 层关键差异

#### Cortex-M0 (`port/cortex_m0/`)

**无 BASEPRI**: M0 只有 PRIMASK 寄存器，无法实现基于 BASEPRI 的中断嵌套。`port_m0.c` 中只有 PRIMASK 版本的临界区代码，且通过 `#error` 阻止 `OS_CONFIG_USE_INTERRUPT_NESTING=1` 的编译。

**PendSV_Handler**: M0 不支持 `stmdb` / `ldmia` 后递增的语法，需要用 `sub` + `stmia` 手动模拟。R8-R11 需要通过 R4-R7 中转保存。

**NVIC 差异**: M0 只有 1 个 ISER/ICER 寄存器（支持 32 个中断），IP 寄存器只有 8 个。

#### Cortex-M4 (`port/cortex_m4/`)

**FPU 上下文**: PendSV_Handler 在保存/恢复 R4-R11 前后，通过 `tst r14, #0x10` 检查 EXC_RETURN 的 bit 4。如果为 0，表示任务使用了 FPU，需要额外保存/恢复 S16-S31（16 个 32 位寄存器 = 64 字节）。

**FPU 使能**: `prv_fpu_enable()` 在 `os_kernel_init()` 中调用，设置 SCB.CPACR 的 CP10/CP11 为 full access。

#### Cortex-M7 (`port/cortex_m7/`)

**双精度 FPU**: M7 的 FPU 支持双精度浮点，D16-D31 是 16 个 64 位寄存器（32 个 32 位字）。PendSV 使用 `vstmdbeq r0!, {d16-d31}` / `vldmiaeq r0!, {d16-d31}` 保存/恢复，占用 32 word 栈空间（vs M4 的 16 word）。

### 25.3 如何适配新芯片

要将 MiniRTOS 移植到一个新的 Cortex-M 芯片，需要修改以下文件：

1. **`port.c`**: 修改 `os_port_systick_init()` 中的时钟频率常量
2. **`startup_*.s`**: 修改 `.cpu` 和 `.fpu` 指令、向量表中的中断列表
3. **`*.ld`**: 修改 Flash/RAM 地址和大小
4. **`os_config.h`**: 修改 `OS_CONFIG_NVIC_PRIO_BITS` 和 `OS_CONFIG_USE_INTERRUPT_NESTING`

**步骤**:
1. 复制最接近的 port 目录（如 `port/cortex_m4/`）
2. 查阅芯片数据手册，确认时钟频率、Flash/RAM 大小、中断列表
3. 修改上述 4 个文件
4. 更新 Makefile 中的 `C_SOURCES`、`ASM_SOURCES`、`LDSCRIPT`、`CPU`、`FPU` 变量

### 25.4 目录结构

```
port/
├── port.c / port.h                  # Cortex-M3 (STM32F103, 72MHz)
├── startup_stm32f103.s
├── stm32f103.ld
├── cortex_m0/
│   ├── port_m0.c                    # Cortex-M0 (STM32F030, 48MHz, 无 FPU/BASEPRI)
│   ├── startup_stm32f030.s
│   └── stm32f030.ld
├── cortex_m4/
│   ├── port_m4.c                    # Cortex-M4 (STM32F407, 168MHz, 单精度 FPU)
│   ├── startup_stm32f407.s
│   └── stm32f407.ld
└── cortex_m7/
    ├── port_m7.c                    # Cortex-M7 (STM32F746, 216MHz, 双精度 FPU)
    ├── startup_stm32f746.s
    └── stm32f746.ld
```

### 26.1 新增字段

为支持同步原语的阻塞/超时机制，TCB 新增以下字段：

```c
/* 阻塞对象追踪 */
void    *blocked_on;        // 阻塞在哪个对象上
uint8_t blocked_reason;     // 阻塞原因 (os_blocked_reason_t)
uint8_t timed_out;          // 超时标志 (tick 处理器设置)

/* 事件组等待状态 */
uint32_t event_wait_bits;
uint32_t event_wait_options;
uint32_t event_return_bits;
```

### 26.2 阻塞原因枚举

```c
typedef enum {
    OS_BLOCKED_NONE             = 0,
    OS_BLOCKED_ON_QUEUE_SEND    = 1,
    OS_BLOCKED_ON_QUEUE_RECV    = 2,
    OS_BLOCKED_ON_SEM_TAKE      = 3,
    OS_BLOCKED_ON_MUTEX_LOCK    = 4,
    OS_BLOCKED_ON_EVENT_WAIT    = 5,
} os_blocked_reason_t;
```

### 26.3 超时处理流程

1. 任务阻塞时设置 `blocked_on`、`blocked_reason`、`delay_ticks`
2. `os_task_tick()` 递减 `delay_ticks`
3. 到期时设置 `timed_out = 1`，清除 `blocked_on`
4. 任务移入就绪链表
5. API 函数检查 `timed_out` 返回 `OS_ERR_TIMEOUT`

---

## 附录 A: 术语表

| 术语 | 含义 |
|------|------|
| TCB | Task Control Block，任务控制块 |
| PSP | Process Stack Pointer，进程栈指针 |
| MSP | Main Stack Pointer，主栈指针 |
| PendSV | Pending Service，挂起服务异常 |
| SysTick | 系统滴答定时器 |
| PRIMASK | 中断屏蔽寄存器 (bit 0 = 1 时禁用中断) |
| EXC_RETURN | 异常返回值 (LR 中的特殊值) |
| AAPCS | ARM Architecture Procedure Call Standard |
| ISR | Interrupt Service Routine，中断服务程序 |
| Coalescing | 相邻空闲块合并 |
| First-Fit | 首次适配算法 |
| High-Water Mark | 高水位标记 (历史最大使用量) |
| WFI | Wait For Interrupt，等待中断 (低功耗指令) |
| xPSR | Program Status Register (包含 Thumb 模式位) |
| Mutex | Mutual Exclusion，互斥锁 |
| Semaphore | 信号量，用于同步和资源计数 |
| Queue | 消息队列，用于任务间通信 |
| Event Group | 事件标志组，多条件同步 |
| Priority Inheritance | 优先级继承，防止优先级反转 |
| Delta List | 差值链表，定时器按相对时间排序 |
| Ring Buffer | 环形缓冲区，队列的底层数据结构 |

---

## 附录 B: 参考资料

1. **FreeRTOS 官方文档**: https://www.freertos.org/Documentation/RTOS_book.html
2. **ARM Cortex-M3 技术参考手册**: ARM DDI 0337I
3. **STM32F103 参考手册**: RM0008
4. **ARM AAPCS**: ARM IHI 0042F (过程调用标准)
5. **《嵌入式实时操作系统 uC/OS-II》**: Jean J. Labrosse 著
