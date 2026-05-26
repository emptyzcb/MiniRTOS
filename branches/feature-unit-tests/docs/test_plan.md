# MiniRTOS 测试方案

**版本**: v1.0
**适用版本**: MiniRTOS v0.6.0

---

## 目录

1. [测试策略](#1-测试策略)
2. [单元测试框架](#2-单元测试框架)
3. [单元测试用例](#3-单元测试用例)
4. [集成测试](#4-集成测试)
5. [移植层测试](#5-移植层测试)
6. [测试执行流程](#6-测试执行流程)
7. [测试覆盖率目标](#7-测试覆盖率目标)

---

## 1. 测试策略

### 1.1 分层测试

```
┌─────────────────────────────────────────────┐
│           系统集成测试 (Target)              │  ← 在 STM32 硬件上运行
│   多任务协作、中断响应、长时间稳定性         │
├─────────────────────────────────────────────┤
│           模块集成测试 (Target)              │  ← 在 STM32 硬件上运行
│   调度器+任务、队列+信号量联动               │
├─────────────────────────────────────────────┤
│           单元测试 (Host)                    │  ← 在 PC 上运行
│   每个模块独立验证，mock 硬件层              │
└─────────────────────────────────────────────┘
```

### 1.2 测试环境

| 环境 | 平台 | 编译器 | 用途 |
|------|------|--------|------|
| Host 测试 | x86_64 Linux/Windows | GCC / MinGW | 单元测试，快速迭代 |
| Target 测试 | STM32F103C8T6 | arm-none-eabi-gcc | 集成测试，硬件验证 |

### 1.3 Mock 策略

Host 端单元测试需要 mock 硬件相关函数：

```c
/* mock_port.h - Host 端移植层 mock */
#ifndef MOCK_PORT_H
#define MOCK_PORT_H

#include <stdint.h>

/* Mock 临界区 (无操作) */
static inline uint32_t mock_port_enter_critical(void) { return 0; }
static inline void mock_port_exit_critical(uint32_t x) { (void)x; }

/* Mock 上下文切换计数器 */
extern uint32_t mock_context_switch_count;
extern uint32_t mock_current_tick;

/* Mock port 函数声明 */
void os_port_enter_critical(void);
void os_port_exit_critical(void);
os_stack_t* os_port_stack_init(os_task_func_t func, void *param,
                                os_stack_t *stack_base, uint32_t stack_size);

#endif
```

---

## 2. 单元测试框架

### 2.1 轻量级测试框架

MiniRTOS 使用自定义的轻量级测试框架，零依赖，适合嵌入式项目。

**文件结构**：

```
tests/
├── test_framework.h       # 测试框架头文件
├── test_framework.c       # 测试框架实现
├── mock_port.c            # 硬件 mock 层
├── test_heap4.c           # Heap-4 单元测试
├── test_task.c            # 任务管理单元测试
├── test_scheduler.c       # 调度器单元测试
├── test_queue.c           # 消息队列单元测试
├── test_semaphore.c       # 信号量单元测试
├── test_mutex.c           # 互斥锁单元测试
├── test_timer.c           # 软件定时器单元测试
├── test_eventgroup.c      # 事件标志组单元测试
├── test_mempool.c         # 内存池单元测试
├── test_mailbox.c         # 邮箱单元测试
├── test_notify.c          # 任务通知单元测试
├── test_watchdog.c        # 软件看门狗单元测试
├── test_main.c            # 测试入口
└── Makefile               # Host 端构建
```

### 2.2 测试框架 API

```c
/* test_framework.h */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

/* 测试计数器 */
static uint32_t tests_run    = 0;
static uint32_t tests_passed = 0;
static uint32_t tests_failed = 0;

/* 断言宏 */
#define TEST_ASSERT(cond) \
    do { \
        tests_run++; \
        if (cond) { tests_passed++; } \
        else { \
            tests_failed++; \
            printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    TEST_ASSERT((expected) == (actual))

#define TEST_ASSERT_NOT_NULL(ptr) \
    TEST_ASSERT((ptr) != NULL)

#define TEST_ASSERT_NULL(ptr) \
    TEST_ASSERT((ptr) == NULL)

/* 测试用例注册 */
#define TEST_CASE(name) static void name(void)

#define RUN_TEST(name) \
    do { \
        printf("  [RUN ] %s\n", #name); \
        name(); \
    } while(0)

/* 测试套件 */
#define TEST_SUITE_BEGIN(name) \
    void test_suite_##name(void) { \
        printf("\n=== Test Suite: %s ===\n", #name);

#define TEST_SUITE_END() \
        printf("=== Results: %u/%u passed, %u failed ===\n", \
               tests_passed, tests_run, tests_failed); \
    }

/* 测试报告 */
static inline int test_report(void) {
    printf("\n=============================\n");
    printf("Total: %u | Passed: %u | Failed: %u\n",
           tests_run, tests_passed, tests_failed);
    printf("=============================\n");
    return (tests_failed > 0) ? 1 : 0;
}

#endif
```

---

## 3. 单元测试用例

### 3.1 Heap-4 测试 (`test_heap4.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| H-01 | `test_heap_init` | 初始化后检查空闲大小 | free_size == HEAP_SIZE |
| H-02 | `test_heap_alloc_basic` | 分配一块内存 | 返回非 NULL |
| H-03 | `test_heap_alloc_zero` | 分配 0 字节 | 返回 NULL |
| H-04 | `test_heap_alloc_exact` | 分配恰好等于剩余空间 | 成功，free_size == 0 |
| H-05 | `test_heap_alloc_oversize` | 分配超过堆大小 | 返回 NULL |
| H-06 | `test_heap_free_basic` | 分配后释放 | free_size 恢复 |
| H-07 | `test_heap_free_null` | 释放 NULL | 不崩溃 |
| H-08 | `test_heap_coalesce` | 释放相邻块后合并 | 最大空闲块增大 |
| H-09 | `test_heap_split` | 分配小块时分割大块 | 产生新空闲块 |
| H-10 | `test_heap_realloc_expand` | realloc 扩大 | 数据完整 |
| H-11 | `test_heap_realloc_shrink` | realloc 缩小 | 数据完整 |
| H-12 | `test_heap_calloc_zero` | calloc 内容为零 | 所有字节为 0 |
| H-13 | `test_heap_min_free` | 多次分配后检查 min_free | 记录历史最低 |
| H-14 | `test_heap_fragmentation` | 交替分配释放不同大小 | 无内存泄漏 |
| H-15 | `test_heap_alignment` | 检查返回地址对齐 | 地址 8 字节对齐 |

```c
/* test_heap4.c 示例 */
#include "test_framework.h"
#include "heap4.h"

TEST_CASE(test_heap_init) {
    os_heap_init();
    uint32_t free_size = os_heap_get_free_size();
    TEST_ASSERT_EQUAL(OS_CONFIG_HEAP_SIZE, free_size);
}

TEST_CASE(test_heap_alloc_basic) {
    os_heap_init();
    void *p = os_heap_alloc(128);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT(os_heap_get_free_size() < OS_CONFIG_HEAP_SIZE);
    os_heap_free(p);
}

TEST_CASE(test_heap_coalesce) {
    os_heap_init();
    void *a = os_heap_alloc(256);
    void *b = os_heap_alloc(256);
    void *c = os_heap_alloc(256);

    os_heap_free(a);
    os_heap_free(c);
    uint32_t before = os_heap_get_largest_free_block();

    os_heap_free(b);  /* a+b+c 应合并 */
    uint32_t after = os_heap_get_largest_free_block();

    TEST_ASSERT(after > before);
}

void test_suite_heap4(void) {
    printf("\n=== Test Suite: Heap-4 ===\n");
    RUN_TEST(test_heap_init);
    RUN_TEST(test_heap_alloc_basic);
    RUN_TEST(test_heap_coalesce);
    /* ... */
}
```

### 3.2 任务管理测试 (`test_task.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| T-01 | `test_task_create` | 创建任务 | 返回 OS_OK，handle 非 NULL |
| T-02 | `test_task_create_invalid_prio` | 优先级超出范围 | 返回 OS_ERR_PARAM |
| T-03 | `test_task_create_null_func` | 函数指针为 NULL | 返回 OS_ERR_PARAM |
| T-04 | `test_task_create_stack_too_small` | 栈小于 128 字节 | 返回 OS_ERR_PARAM |
| T-05 | `test_task_create_max_tasks` | 创建超过 MAX_TASKS | 返回 OS_ERR_FULL |
| T-06 | `test_task_delete_other` | 删除其他任务 | 返回 OS_OK，任务数减 1 |
| T-07 | `test_task_suspend_resume` | 挂起再恢复 | 状态变化正确 |
| T-08 | `test_task_set_priority` | 修改优先级 | 新优先级生效 |
| T-09 | `test_task_get_state` | 查询任务状态 | 返回正确状态 |
| T-10 | `test_task_get_count` | 查询任务总数 | 计数正确 |
| T-11 | `test_task_stack_high_water` | 栈使用峰值检测 | 值合理 |
| T-12 | `test_task_delay` | 延时阻塞 | delay_ticks 设置正确 |
| T-13 | `test_task_create_suspended` | 创建挂起态任务 | 初始状态为 SUSPENDED |
| T-14 | `test_task_ready_list_order` | 就绪链表优先级顺序 | 高优先级在前 |

### 3.3 调度器测试 (`test_scheduler.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| S-01 | `test_sched_critical_nesting` | 临界区嵌套 | 嵌套计数正确 |
| S-02 | `test_sched_select_highest` | 选择最高优先级 | 选择正确任务 |
| S-03 | `test_sched_time_slice` | 时间片轮转 | 同优先级轮流执行 |
| S-04 | `test_sched_yield` | 主动让出 CPU | 切换到下一个任务 |

### 3.4 消息队列测试 (`test_queue.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| Q-01 | `test_queue_create` | 创建队列 | 返回 OS_OK |
| Q-02 | `test_queue_send_receive` | 发送接收基本操作 | 数据一致 |
| Q-03 | `test_queue_full` | 队列满时发送 | 返回 OS_ERR_FULL |
| Q-04 | `test_queue_empty` | 队列空时接收 | 返回 OS_ERR_EMPTY |
| Q-05 | `test_queue_fifo_order` | FIFO 顺序 | 接收顺序与发送一致 |
| Q-06 | `test_queue_peek` | 查看不取出 | 数据仍在队列中 |
| Q-07 | `test_queue_count_spaces` | 计数查询 | count + spaces == max_items |
| Q-08 | `test_queue_wraparound` | 环形缓冲区回绕 | 数据正确 |
| Q-09 | `test_queue_delete` | 删除队列 | 资源释放 |
| Q-10 | `test_queue_item_size_4` | 4 字节元素 | 数据完整 |
| Q-11 | `test_queue_item_size_64` | 64 字节元素 | 数据完整 |

### 3.5 信号量测试 (`test_semaphore.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| SM-01 | `test_sem_create_binary` | 创建二值信号量 | 初始值为 0 |
| SM-02 | `test_sem_create_counting` | 创建计数信号量 | 初始值正确 |
| SM-03 | `test_sem_give_take` | 获取释放操作 | 计数变化正确 |
| SM-04 | `test_sem_take_empty` | 空信号量获取 | 返回 OS_ERR_EMPTY |
| SM-05 | `test_sem_give_max` | 超过最大值释放 | 返回 OS_ERR_FULL |
| SM-06 | `test_sem_binary_limit` | 二值信号量上限 | 最大值为 1 |
| SM-07 | `test_sem_delete` | 删除信号量 | 资源释放 |

### 3.6 互斥锁测试 (`test_mutex.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| MX-01 | `test_mutex_create` | 创建互斥锁 | 返回 OS_OK |
| MX-02 | `test_mutex_lock_unlock` | 加锁解锁 | 状态正确 |
| MX-03 | `test_mutex_recursive` | 递归加锁 | lock_count 递增 |
| MX-04 | `test_mutex_unlock_not_owner` | 非持有者解锁 | 返回 OS_ERR_STATE |
| MX-05 | `test_mutex_priority_inherit` | 优先级继承 | 持有者优先级提升 |
| MX-06 | `test_mutex_priority_restore` | 优先级恢复 | 解锁后恢复原优先级 |
| MX-07 | `test_mutex_delete` | 删除互斥锁 | 资源释放 |

### 3.7 软件定时器测试 (`test_timer.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| TM-01 | `test_timer_create` | 创建定时器 | 返回 OS_OK |
| TM-02 | `test_timer_one_shot` | 单次定时器 | 回调执行一次 |
| TM-03 | `test_timer_auto_reload` | 自动重载定时器 | 回调周期执行 |
| TM-04 | `test_timer_stop` | 停止定时器 | 不再触发 |
| TM-05 | `test_timer_reset` | 重置定时器 | 计时重新开始 |
| TM-06 | `test_timer_change_period` | 修改周期 | 新周期生效 |
| TM-07 | `test_timer_is_active` | 查询激活状态 | 状态正确 |

### 3.8 事件标志组测试 (`test_eventgroup.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| EG-01 | `test_eventgroup_create` | 创建事件组 | 返回 OS_OK |
| EG-02 | `test_eventgroup_set_get` | 设置查询位 | 位值正确 |
| EG-03 | `test_eventgroup_clear` | 清除位 | 指定位清零 |
| EG-04 | `test_eventgroup_wait_any` | 等待任意位 | 满足即返回 |
| EG-05 | `test_eventgroup_wait_all` | 等待全部位 | 全部满足才返回 |
| EG-06 | `test_eventgroup_clear_on_exit` | 退出时清除 | 匹配位被清除 |
| EG-07 | `test_eventgroup_delete` | 删除事件组 | 资源释放 |

### 3.9 内存池测试 (`test_mempool.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| MP-01 | `test_mempool_create` | 创建内存池 | 返回 OS_OK |
| MP-02 | `test_mempool_alloc_free` | 分配释放 | 块正确回收 |
| MP-03 | `test_mempool_exhaust` | 耗尽所有块 | 返回 NULL |
| MP-04 | `test_mempool_owns` | 检查指针归属 | 正确识别 |
| MP-05 | `test_mempool_min_free` | 高水位记录 | 值正确 |
| MP-06 | `test_mempool_alignment` | 块地址对齐 | 满足指针对齐 |

### 3.10 邮箱测试 (`test_mailbox.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| MB-01 | `test_mailbox_create` | 创建邮箱 | 返回 OS_OK |
| MB-02 | `test_mailbox_send_receive` | 发送接收 | 数据一致 |
| MB-03 | `test_mailbox_full` | 满时发送 | 行为正确 |
| MB-04 | `test_mailbox_empty` | 空时接收 | 返回 OS_ERR_EMPTY |
| MB-05 | `test_mailbox_overwrite` | 覆盖写入 | 旧数据被替换 |

### 3.11 任务通知测试 (`test_notify.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| NT-01 | `test_notify_send` | 发送通知 | 值正确传递 |
| NT-02 | `test_notify_wait` | 等待通知 | 收到正确值 |
| NT-03 | `test_notify_overwrite` | 多次发送覆盖 | 只保留最新值 |

### 3.12 软件看门狗测试 (`test_watchdog.c`)

| 编号 | 测试用例 | 描述 | 预期结果 |
|------|---------|------|---------|
| WD-01 | `test_wdt_register` | 注册看门狗 | 返回 OS_OK |
| WD-02 | `test_wdt_feed` | 喂狗操作 | 计时器重置 |
| WD-03 | `test_wdt_timeout` | 超时检测 | 回调被触发 |
| WD-04 | `test_wdt_unregister` | 注销看门狗 | 不再监控 |

---

## 4. 集成测试

集成测试在 STM32 硬件上运行，验证模块间协作。

### 4.1 调度集成测试

| 编号 | 测试用例 | 描述 | 验证方法 |
|------|---------|------|---------|
| IT-01 | 抢占调度 | 高优先级任务抢占低优先级 | Trace 记录切换顺序 |
| IT-02 | 时间片轮转 | 同优先级轮流执行 | 每个任务执行计数相近 |
| IT-03 | 延时唤醒 | 任务延时后准时唤醒 | 检查实际 tick 误差 |
| IT-04 | 嵌套中断 | ISR 中唤醒高优先级任务 | 中断返回后立即切换 |

### 4.2 IPC 集成测试

| 编号 | 测试用例 | 描述 | 验证方法 |
|------|---------|------|---------|
| IT-05 | 生产者-消费者 | 队列+信号量协作 | 数据完整无丢失 |
| IT-06 | 优先级继承 | 互斥锁防止优先级反转 | 监控任务响应时间 |
| IT-07 | 事件组合 | 多条件同步 | 所有位满足后唤醒 |
| IT-08 | 任务通知 | 轻量级信号传递 | 值正确传递 |

### 4.3 稳定性测试

| 编号 | 测试用例 | 描述 | 验证方法 |
|------|---------|------|---------|
| IT-09 | 长时间运行 | 连续运行 24 小时 | 无内存泄漏、无崩溃 |
| IT-10 | 高负载 | 多任务高频切换 | 无死锁、栈无溢出 |
| IT-11 | 堆碎片 | 长时间随机分配释放 | 堆仍可正常使用 |
| IT-12 | 看门狗恢复 | 任务卡死时触发回调 | 系统继续运行 |

### 4.4 集成测试用例示例

```c
/* integration_test.c - 生产者消费者集成测试 */
#include "os.h"

#define TEST_ITERATIONS  1000

static os_queue_t test_queue;
static os_sem_t test_sem;
static volatile uint32_t produced_count = 0;
static volatile uint32_t consumed_count = 0;
static volatile uint32_t data_error = 0;

static os_stack_t prod_stack[128];
static os_stack_t cons_stack[128];

static void producer(void *param) {
    (void)param;
    for (uint32_t i = 1; i <= TEST_ITERATIONS; i++) {
        os_queue_send(&test_queue, &i, OS_WAIT_FOREVER);
        os_sem_give(&test_sem);
        produced_count++;
    }
    /* 测试完成后挂起自己 */
    os_task_suspend(NULL);
}

static void consumer(void *param) {
    (void)param;
    uint32_t expected = 1;
    while (1) {
        os_sem_take(&test_sem, OS_WAIT_FOREVER);
        uint32_t received;
        if (os_queue_receive(&test_queue, &received, OS_WAIT_FOREVER) == OS_OK) {
            if (received != expected) {
                data_error++;
            }
            expected++;
            consumed_count++;
        }
    }
}

void integration_test_producer_consumer(void) {
    os_kernel_init();
    os_queue_create(&test_queue, sizeof(uint32_t), 10);
    os_sem_create_binary(&test_sem);

    os_task_create(producer, "PROD", NULL, 2, prod_stack, sizeof(prod_stack), NULL);
    os_task_create(consumer, "CONS", NULL, 3, cons_stack, sizeof(cons_stack), NULL);

    os_kernel_start();
    /* 不会到达这里 */
}

/* 验证函数 (在监控任务中调用) */
void verify_test_result(void) {
    OS_ASSERT(produced_count == TEST_ITERATIONS);
    OS_ASSERT(consumed_count == TEST_ITERATIONS);
    OS_ASSERT(data_error == 0);
}
```

---

## 5. 移植层测试

### 5.1 各 Port 通用测试

| 编号 | 测试用例 | 描述 | 验证方法 |
|------|---------|------|---------|
| PT-01 | 栈帧初始化 | 检查初始栈帧内容 | 读取栈内存验证 |
| PT-02 | 上下文切换 | 多任务交替执行 | 每个任务独立计数 |
| PT-03 | SysTick 精度 | 1ms 定时精度 | 与参考时钟对比 |
| PT-04 | 临界区 | 关中断保护 | 共享变量无竞争 |
| PT-05 | 第一次启动 | 从裸机到多任务 | 系统正常运行 |

### 5.2 Cortex-M4/M7 特有测试

| 编号 | 测试用例 | 描述 | 验证方法 |
|------|---------|------|---------|
| PT-06 | FPU 上下文保存 | 浮点运算后切换 | 浮点变量值正确 |
| PT-07 | 双精度 FPU (M7) | double 运算 | 精度无损 |

```c
/* fpu_test.c - FPU 上下文切换测试 */
static os_stack_t fp_stack1[128];
static os_stack_t fp_stack2[128];

static volatile float result1 = 0;
static volatile float result2 = 0;

static void fp_task1(void *param) {
    (void)param;
    float acc = 1.0f;
    for (int i = 0; i < 1000; i++) {
        acc *= 1.001f;
        if ((i % 100) == 0) os_task_delay(1);  /* 触发上下文切换 */
    }
    result1 = acc;  /* 预期值: ~2.717 */
}

static void fp_task2(void *param) {
    (void)param;
    double acc = 1.0;
    for (int i = 0; i < 1000; i++) {
        acc *= 1.001;
        if ((i % 100) == 0) os_task_delay(1);
    }
    result2 = (float)acc;
}
```

---

## 6. 测试执行流程

### 6.1 Host 端单元测试

```bash
# 进入测试目录
cd D:\A_stm32_project\miniRTOS\tests

# 编译并运行所有单元测试
make test

# 运行指定模块测试
make test_heap4
make test_task
make test_queue

# 查看测试报告
make test_report
```

### 6.2 Target 端集成测试

```bash
# 编译集成测试固件
make TEST=integration flash

# 通过串口查看输出
make monitor

# 运行稳定性测试 (24小时)
make TEST=stability flash
```

### 6.3 完整测试流程

```
┌─────────────────────────────────────────────┐
│  1. 代码提交                                 │
│     git commit / push                        │
├─────────────────────────────────────────────┤
│  2. Host 单元测试                            │
│     make test                                │
│     ├── test_heap4      ✓/✗                  │
│     ├── test_task       ✓/✗                  │
│     ├── test_queue      ✓/✗                  │
│     ├── test_semaphore  ✓/✗                  │
│     ├── test_mutex      ✓/✗                  │
│     ├── test_timer      ✓/✗                  │
│     ├── test_eventgroup ✓/✗                  │
│     ├── test_mempool    ✓/✗                  │
│     ├── test_mailbox    ✓/✗                  │
│     ├── test_notify     ✓/✗                  │
│     └── test_watchdog   ✓/✗                  │
├─────────────────────────────────────────────┤
│  3. Target 编译检查                          │
│     make all                                 │
│     ├── STM32F103 (Cortex-M3)  ✓/✗          │
│     ├── STM32F030 (Cortex-M0)  ✓/✗          │
│     ├── STM32F407 (Cortex-M4)  ✓/✗          │
│     └── STM32F746 (Cortex-M7)  ✓/✗          │
├─────────────────────────────────────────────┤
│  4. 硬件集成测试                             │
│     make TEST=integration flash              │
│     ├── 调度测试    ✓/✗                      │
│     ├── IPC 测试    ✓/✗                      │
│     └── 稳定性测试  ✓/✗                      │
├─────────────────────────────────────────────┤
│  5. 测试报告                                 │
│     全部通过 → 合并/发布                      │
│     存在失败 → 修复后重新测试                 │
└─────────────────────────────────────────────┘
```

---

## 7. 测试覆盖率目标

| 模块 | 行覆盖率目标 | 分支覆盖率目标 |
|------|-------------|---------------|
| heap4.c | ≥ 90% | ≥ 80% |
| task.c | ≥ 85% | ≥ 75% |
| scheduler.c | ≥ 90% | ≥ 80% |
| queue.c | ≥ 90% | ≥ 85% |
| semaphore.c | ≥ 90% | ≥ 85% |
| mutex.c | ≥ 90% | ≥ 85% |
| timer.c | ≥ 85% | ≥ 75% |
| eventgroup.c | ≥ 85% | ≥ 75% |
| mempool.c | ≥ 90% | ≥ 85% |
| port.c | ≥ 70% | ≥ 60% |

**覆盖率工具**：Host 端使用 `gcov` + `lcov` 生成覆盖率报告。

```bash
# 生成覆盖率报告
make coverage
# 打开 HTML 报告
open coverage/index.html
```

---

## 附录 A: Makefile 示例 (Host 测试)

```makefile
# tests/Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -std=c11
CFLAGS += -I../config -I../common -I../include -I../kernel
CFLAGS += -DTEST_HOST_BUILD

# 测试源文件
TEST_SRC = test_framework.c mock_port.c \
           test_heap4.c test_task.c test_queue.c \
           test_semaphore.c test_mutex.c test_timer.c \
           test_eventgroup.c test_mempool.c test_mailbox.c \
           test_notify.c test_watchdog.c test_main.c

# 被测模块 (从 kernel 目录)
KERNEL_SRC = ../kernel/heap4.c ../kernel/task.c ../kernel/scheduler.c \
             ../kernel/kernel.c ../kernel/queue.c ../kernel/semaphore.c \
             ../kernel/mutex.c ../kernel/timer.c ../kernel/eventgroup.c \
             ../kernel/mempool.c ../kernel/mailbox.c ../kernel/notify.c \
             ../kernel/watchdog.c

.PHONY: test clean

test: test_runner
	./test_runner

test_runner: $(TEST_SRC) $(KERNEL_SRC)
	$(CC) $(CFLAGS) -o $@ $^

coverage: CFLAGS += --coverage
coverage: test
	gcovr -r .. --html -o coverage/index.html

clean:
	rm -f test_runner *.o *.gcda *.gcno
	rm -rf coverage
```

---

## 附录 B: CI/CD 配置示例

```yaml
# .github/workflows/test.yml
name: MiniRTOS CI

on: [push, pull_request]

jobs:
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get install gcc-arm-none-eabi gcovr
      - name: Run unit tests
        run: cd tests && make test
      - name: Generate coverage
        run: cd tests && make coverage
      - name: Upload coverage
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: tests/coverage/

  target-build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [stm32f103, stm32f030, stm32f407, stm32f746]
    steps:
      - uses: actions/checkout@v4
      - name: Install toolchain
        run: sudo apt-get install gcc-arm-none-eabi
      - name: Build
        run: make TARGET=${{ matrix.target }} all
```
