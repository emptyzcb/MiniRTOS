# MiniRTOS

一个面向 ARM Cortex-M 的轻量级嵌入式实时操作系统，参考 FreeRTOS 核心架构设计，用于学习 RTOS 内核原理和嵌入式开发。

## 特性

- 抢占式调度 + 时间片轮转
- 任务管理、消息队列、信号量、互斥锁
- 软件定时器、事件标志组、内存池、Heap-4 内存管理
- 支持 Cortex-M0 / M3 / M4 / M7 移植
- 纯 C 编写，单个头文件 `os.h` 即可使用

## 快速开始

```bash
make            # 编译
make flash      # 烧录（需 OpenOCD）
```

示例入口见 `app/main.c`，移植代码见 `port/`，配置在 `config/os_config.h`。

## 目录结构

```
config/   内核配置
common/   通用类型定义
include/  对外统一头文件
kernel/   内核模块（任务、调度、队列等）
port/     平台移植（Cortex-M0/M3/M4/M7）
app/      应用示例
tests/    单元测试
docs/     设计文档
```

## 文档

- [技术设计文档](docs/miniRTOS_technical_design.md)
- [测试计划](docs/test_plan.md)
