# MiniOS - 轻量级嵌入式实时操作系统

## 项目概述

MiniOS 是一个面向 ARM Cortex-M3 (STM32F103) 的轻量级嵌入式实时操作系统，设计参考 FreeRTOS 核心架构。用于学习 RTOS 内核原理和嵌入式系统开发。

- **目标平台**: STM32F103C8T6 (Cortex-M3, 72MHz, 64KB Flash, 20KB RAM)
- **语言**: C11 + ARM Assembly
- **构建工具**: arm-none-eabi-gcc + Make
- **版本**: v0.6.0 (新增 Cortex-M0/M4/M7 移植层)

---

## 项目结构

```
D:\A_stm32_project\
├── config\
│   └── os_config.h           # 内核配置宏 (任务数、堆大小、tick频率、功能开关)
├── common\
│   └── os_types.h            # 通用类型定义 (状态码、任务句柄、阻塞原因枚举)
├── include\
│   └── os.h                  # 对外统一头文件 (应用层只需包含此文件)
├── kernel\
│   ├── heap4.h / heap4.c     # Heap-4 内存管理 (首次适配+合并空闲块)
│   ├── task.h / task.c       # 任务管理 (TCB、就绪链表、tick处理)
│   ├── scheduler.h / .c      # 调度器 (优先级抢占+时间片轮转)
│   ├── kernel.h / kernel.c   # 内核初始化、系统tick管理
│   ├── queue.h / queue.c     # 消息队列 (环形缓冲区+阻塞收发)
│   ├── semaphore.h / .c      # 信号量 (二值/计数)
│   ├── mutex.h / mutex.c     # 互斥锁 (优先级继承)
│   ├── timer.h / timer.c     # 软件定时器 (单次/自动重载)
│   ├── eventgroup.h / .c     # 事件标志组 (等待任意/全部位)
│   └── mempool.h / mempool.c # 内存池分配器 (固定大小块, O(1) 分配释放)
├── port\
│   ├── port.h / port.c       # Cortex-M3 移植层 (PendSV上下文切换、SysTick)
│   ├── startup_stm32f103.s   # 启动文件 (向量表、.data/.bss初始化)
│   └── stm32f103.ld          # 链接脚本 (Flash/RAM布局)
├── app\
│   └── main.c                # 应用入口 (演示所有功能)
├── Makefile                  # 构建系统
├── .gitignore
└── docs\
    ├── build_report.md       # 本文档
    └── miniRTOS_technical_design.md  # 技术设计文档
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
os_status_t os_task_delay_until(previous_wake_tick, period_ticks);
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
2. `os_timer_init()` - 初始化定时器子系统 (创建服务任务)
3. `os_sched_select_next()` - 选择第一个任务
4. `os_port_start_first_task()` - 启动第一个任务 (永不返回)

**Tick 处理链**:
1. `os_kernel_tick_increment()` - 递增系统 tick
2. `os_task_tick()` - 处理任务延时、栈溢出检测、延迟删除
3. `os_timer_tick()` - 处理软件定时器到期
4. `os_sched_select_next()` - 选择下一个任务

---

### 6. 消息队列 (`kernel/queue.c`)

**用途**: 任务间通信 (IPC)，支持阻塞发送/接收和 ISR 安全变体。

**数据结构**: 环形缓冲区 + 优先级排序的阻塞等待链表。

```c
typedef struct os_queue {
    uint8_t     *buffer;        // 环形缓冲区 (堆分配)
    uint32_t    item_size;      // 每个元素大小
    uint32_t    max_items;      // 最大元素数
    uint32_t    count;          // 当前元素数
    uint32_t    head, tail;     // 读/写索引
    os_tcb_t    *send_wait_list;  // 发送等待链表
    os_tcb_t    *recv_wait_list;  // 接收等待链表
} os_queue_t;
```

**API**:
```c
os_queue_create(queue, item_size, max_items);
os_queue_delete(queue);
os_queue_reset(queue);                        // 清空队列
os_queue_send(queue, item, timeout);         // 阻塞发送
os_queue_send_from_isr(queue, item);         // ISR 安全发送
os_queue_overwrite(queue, item);             // 单元素队列覆盖写入
os_queue_overwrite_from_isr(queue, item);    // ISR 安全覆盖写入
os_queue_receive(queue, item, timeout);      // 阻塞接收
os_queue_receive_from_isr(queue, item);      // ISR 安全接收
os_queue_peek(queue, item);                  // 非阻塞查看
os_queue_get_count_from_isr(queue);          // ISR 安全读取元素数
os_queue_get_count/get_spaces/is_empty/is_full(queue);
```

**阻塞行为**: 队列满时发送阻塞，队列空时接收阻塞。支持 `OS_WAIT_FOREVER`、`OS_WAIT_NONE` 或指定超时 tick 数。

---

### 7. 信号量 (`kernel/semaphore.c`)

**用途**: 任务同步和资源计数。

**类型**:
- **二值信号量**: max_count = 1，用于任务间同步/通知
- **计数信号量**: max_count > 1，用于资源计数

```c
typedef struct os_sem {
    uint32_t    count;          // 当前计数
    uint32_t    max_count;      // 最大计数
    os_tcb_t    *wait_list;     // 等待链表
} os_sem_t;
```

**API**:
```c
os_sem_create_binary(sem);
os_sem_create_counting(sem, max_count, initial_count);
os_sem_delete(sem);
os_sem_take(sem, timeout);          // 获取 (P 操作)
os_sem_take_from_isr(sem);          // ISR 安全获取
os_sem_give(sem);                   // 释放 (V 操作)
os_sem_give_from_isr(sem);          // ISR 安全释放
os_sem_get_count_from_isr(sem);     // ISR 安全查询计数
```

**关键行为**: `os_sem_give()` 使用直接传递模式 - 如果有等待者，直接唤醒任务而不增加计数。

---

### 8. 互斥锁 (`kernel/mutex.c`)

**用途**: 保护共享资源，防止优先级反转。

**特性**:
- **优先级继承**: 高优先级任务等待时，临时提升持有者优先级
- **递归锁定**: 同一任务可多次加锁，需对应次数解锁

```c
typedef struct os_mutex {
    os_tcb_t    *owner;         // 持有者 (NULL 表示空闲)
    uint32_t    lock_count;     // 锁定深度
    os_prio_t   original_prio;  // 持有者原始优先级
    os_tcb_t    *wait_list;     // 等待链表
} os_mutex_t;
```

**API**:
```c
os_mutex_create(mutex);
os_mutex_delete(mutex);
os_mutex_lock(mutex, timeout);      // 加锁 (支持超时)
os_mutex_unlock(mutex);             // 解锁
os_mutex_get_owner(mutex);          // 查询持有者
os_mutex_is_locked(mutex);          // 查询是否已锁定
os_mutex_get_lock_count(mutex);     // 查询递归锁深度
```

**优先级继承流程**:
1. 任务 A (低优先级) 持有 mutex
2. 任务 B (高优先级) 尝试加锁 -> 阻塞
3. 自动提升 A 的优先级到 B 的级别
4. A 执行完毕解锁 -> 恢复原始优先级
5. B 获得 mutex

---

### 9. 软件定时器 (`kernel/timer.c`)

**用途**: 基于系统 tick 的回调定时器。

**类型**:
- **单次定时器 (ONE_SHOT)**: 到期后自动停止
- **自动重载定时器 (AUTO_RELOAD)**: 到期后自动重新开始

```c
typedef struct os_timer {
    os_timer_callback_t callback;   // 回调函数
    os_tick_t           period;     // 周期 (tick)
    os_tick_t           remaining;  // 剩余 tick
    os_timer_type_t     type;       // 类型
    bool                active;     // 是否激活
    struct os_timer     *next;      // 链表指针
} os_timer_t;
```

**架构**: 使用专用定时器服务任务 (优先级高于空闲任务)。`os_timer_tick()` 在 SysTick 中断中递减定时器，到期时通过信号量唤醒服务任务执行回调。

**API**:
```c
os_timer_create(timer, name, period, type, callback);
os_timer_delete(timer, timeout);
os_timer_start(timer, timeout);
os_timer_stop(timer, timeout);
os_timer_reset(timer, timeout);
os_timer_change_period(timer, new_period, timeout);
os_timer_is_active(timer);
```

---

### 10. 事件标志组 (`kernel/eventgroup.c`)

**用途**: 多条件同步，支持等待任意/全部事件位。

```c
typedef struct os_eventgroup {
    uint32_t    bits;           // 当前事件位 (32 位)
    os_tcb_t    *wait_list;     // 等待链表
} os_eventgroup_t;
```

**等待选项**:
- `OS_EVENT_WAIT_ANY` - 任意位满足即唤醒
- `OS_EVENT_WAIT_ALL` - 所有位满足才唤醒
- `OS_EVENT_CLEAR_ON_EXIT` - 唤醒时清除匹配位

**API**:
```c
os_eventgroup_create(eg);
os_eventgroup_delete(eg);
os_eventgroup_set_bits(eg, bits);           // 设置事件位
os_eventgroup_set_bits_from_isr(eg, bits);  // ISR 安全设置
os_eventgroup_clear_bits(eg, bits);         // 清除事件位
os_eventgroup_clear_bits_from_isr(eg, bits);// ISR 安全清除
os_eventgroup_wait_bits(eg, bits, options, timeout);  // 等待事件位
os_eventgroup_get_bits(eg);                 // 查询当前位
os_eventgroup_get_bits_from_isr(eg);        // ISR 安全查询
```

---

### 11. 内存池 (`kernel/mempool.c`)

**用途**: 固定大小块的 O(1) 分配/释放，零碎片。适合高频分配相同大小对象的场景。

**与 Heap-4 的区别**:
- Heap-4: 任意大小，O(n) 分配，可能碎片化
- Memory Pool: 固定大小，O(1) 分配，零碎片

**数据结构**:
```c
typedef struct os_mempool {
    uint8_t     *pool_start;        // 池起始地址
    uint8_t     *pool_end;          // 池结束地址
    uint32_t    block_size;         // 用户可见的块大小
    uint32_t    total_blocks;       // 总块数
    uint32_t    free_count;         // 当前空闲块数
    uint32_t    min_free_count;     // 空闲块历史最低值
    void        *free_list;         // 空闲链表头 (LIFO)
} os_mempool_t;
```

**核心原理**: 空闲块的前 `sizeof(void*)` 字节存储指向下一个空闲块的指针，无需额外 header。分配取链表头，释放插链表头。

**API**:
```c
os_mempool_create(pool, buf, buf_size, block_size);  // 创建池
os_mempool_alloc(pool);                               // 分配一块 (O(1))
os_mempool_free(pool, ptr);                           // 释放一块 (O(1))
os_mempool_get_free_count(pool);                      // 查询空闲块数
os_mempool_get_min_free_count(pool);                  // 查询高水位
os_mempool_get_total_count(pool);                     // 查询总块数
os_mempool_owns(pool, ptr);                           // 检查指针归属
```

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
| `kernel/task.c` | 任务管理 | heap4.h, scheduler.h, kernel.h, port.h |
| `kernel/scheduler.c` | 调度器 | task.h, kernel.h, port.h |
| `kernel/kernel.c` | 内核核心 | heap4.h, task.h, scheduler.h, timer.h |
| `kernel/queue.c` | 消息队列 | task.h, scheduler.h, heap4.h, port.h |
| `kernel/semaphore.c` | 信号量 | task.h, scheduler.h, port.h |
| `kernel/mutex.c` | 互斥锁 | task.h, scheduler.h, port.h |
| `kernel/timer.c` | 软件定时器 | timer.h, semaphore.h, task.h, heap4.h |
| `kernel/eventgroup.c` | 事件标志组 | task.h, scheduler.h, port.h |
| `kernel/mempool.c` | 内存池分配器 | mempool.h, os_config.h |
| `kernel/mailbox.c` | 邮箱 (单元素队列) | mailbox.h, queue.h |
| `kernel/tickless.c` | Tickless Idle 低功耗 | tickless.h, port.h, kernel.h |
| `kernel/notify.c` | 任务通知 | notify.h, task.h, scheduler.h |
| `kernel/watchdog.c` | 软件看门狗 | watchdog.h, task.h, kernel.h |
| `kernel/stats.c` | CPU 使用统计 | stats.h, task.h, kernel.h |
| `kernel/sysinfo.c` | 系统信息查询 | sysinfo.h, task.h, kernel.h, heap4.h |
| `kernel/trace.c` | Trace 日志 | trace.h, kernel.h, port.h |
| `port/port.c` | Cortex-M3 移植层 | task.h, os_config.h |
| `port/cortex_m0/port_m0.c` | Cortex-M0 移植层 | task.h, os_config.h |
| `port/cortex_m4/port_m4.c` | Cortex-M4 移植层 (FPU) | task.h, os_config.h |
| `port/cortex_m7/port_m7.c` | Cortex-M7 移植层 (双精度 FPU) | task.h, os_config.h |
| `app/main.c` | 应用入口 | os.h |
| `port/startup_stm32f103.s` | 启动汇编 (M3) | stm32f103.ld |

### 内存占用 (v0.6.0 实测)

| 区域 | 大小 | 说明 |
|------|------|------|
| 代码 (.text) | ~16.4 KB | 内核 + 同步原语 + 应用代码 |
| 已初始化数据 (.data) | ~2.1 KB | 全局变量初始值 |
| BSS (.bss) | ~14.4 KB | 堆池 (10KB) + 任务栈 + 定时器栈 |
| **总计 Flash** | **~18.4 KB** | 64KB 的 ~29% (.text + .data) |
| **总计 RAM** | **~16.5 KB** | 20KB 的 ~82% (.data + .bss) |

### 资源预算

- **Flash**: 64KB, 实际使用 ~18.4KB (29%)
- **RAM**: 20KB, 实际使用 ~16.5KB (82%) - 主要是堆池和任务栈
- **最大任务数**: 16
- **堆大小**: 10KB (可配置)
- **定时器服务栈**: 256B

---

## 配置调优

编辑 `config/os_config.h` 修改系统参数:

```c
/* 内核配置 */
#define OS_CONFIG_MAX_TASKS          16     // 最大任务数
#define OS_CONFIG_TICK_RATE_HZ       1000   // 系统节拍 (1ms)
#define OS_CONFIG_DEFAULT_STACK_SIZE  512   // 默认栈大小
#define OS_CONFIG_HEAP_SIZE           (14*1024)  // 堆大小
#define OS_CONFIG_NUM_PRIORITIES      8     // 优先级数
#define OS_CONFIG_USE_TIME_SLICING    1     // 启用时间片轮转
#define OS_CONFIG_TIME_SLICE_TICKS    5     // 时间片长度 (5ms)

/* 功能开关 (可单独禁用以节省 Flash/RAM) */
#define OS_CONFIG_USE_QUEUE             1   // 消息队列
#define OS_CONFIG_USE_SEMAPHORE         1   // 信号量
#define OS_CONFIG_USE_MUTEX             1   // 互斥锁
#define OS_CONFIG_USE_SOFTWARE_TIMERS   1   // 软件定时器
#define OS_CONFIG_USE_EVENTGROUP        1   // 事件标志组
#define OS_CONFIG_TIMER_SERVICE_STACK   256 // 定时器服务栈大小
```

---

## 后续扩展方向

1. ~~**同步原语**: 互斥锁 (Mutex)、信号量 (Semaphore)、事件标志组~~ ✅ 已实现
2. ~~**IPC**: 消息队列 (Queue)~~ ✅ 已实现
3. ~~**软件定时器**: 基于 tick 的回调定时器~~ ✅ 已实现
4. ~~**邮箱 (Mailbox)**: 基于队列的单元素消息传递~~ ✅ 已实现 (v0.5.0)
5. ~~**中断管理**: 优先级分组、中断嵌套~~ ✅ 已实现
6. ~~**内存池**: 固定大小块分配器 (pool allocator)~~ ✅ 已实现
7. ~~**调试工具**: 任务状态查看、运行时统计、Trace 支持~~ ✅ 已实现
8. ~~**低功耗**: Tickless Idle 模式~~ ✅ 已实现 (v0.5.0)
9. ~~**移植**: 扩展到 Cortex-M0/M4/M7~~ ✅ 已实现 (v0.6.0)

---

## 已知问题与注意事项

1. **PendSV_Handler 内联汇编**: 当前使用 GCC 内联汇编实现，正式项目建议使用独立的 `.s` 文件
2. **中断优先级**: STM32 使用 4 位优先级，需确保 PendSV 设为最低优先级
3. **栈对齐**: ARM AAPCS 要求 8 字节栈对齐，`os_port_stack_init` 已处理
4. **current_task_ptr**: 全局变量在 port.c 中定义，供汇编代码直接访问
5. **编译器优化**: `-O2` 下内联汇编中的变量引用可能需要 `volatile` 修饰
