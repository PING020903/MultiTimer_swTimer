swTimer
=======

**一款轻量、可移植的软件定时器库，专为裸机系统设计**

**A lightweight, portable software timer library designed specifically for bare-metal systems.**

---

    swTimer 是一个简洁高效的软件定时器解决方案，适用于无操作系统（bare-metal）的嵌入式应用场景。整个核心逻辑极简，不依赖动态内存，仅需少量资源即可运行，适合在各类微控制器上使用。

    swTimer is a compact and efficient software timer solution designed for bare-metal embedded applications. Its core logic is minimalistic, requiring no dynamic memory allocation and operating with minimal resource overhead, making it suitable for use on a wide range of microcontrollers.



    受到 MultiTimer 设计哲学的启发，swTimer 在保持接口直观的同时，增强了灵活性与控制能力，兼顾实用性与可维护性。

    Inspired by the design philosophy of MultiTimer, swTimer enhances flexibility and control while maintaining an intuitive interface, balancing practicality with maintainability.

---

✅ 主要特性
------

● **极简内核**：定时器调度逻辑简洁清晰，易于理解与移植。

● **句柄抽象**：通过 `swHandle_t` 管理定时器，隐藏内部结构，提升封装性。

● **运行时控制**：支持独立启动、停止和复位定时器，无需重建。

● **动态周期调整**：允许在运行时通过指针或 API 修改定时周期。

● **静态内存管理**：所有定时器对象静态分配，避免堆碎片和内存泄漏。

● **同步机制可选**：支持 SW_LoopAndTickSynchronization 模式，避免主循环与中断 tick 的竞争。

● **错误反馈机制**：提供详细的错误码（如满、无效句柄等），便于调试。

● **MIT 协议开源**：自由使用，兼容商业项目。



## ✅ Key Features

- **Minimalist Kernel**: Timer scheduling logic is concise and clear, making it easy to understand and port.
- **Handle Abstraction**: Manage timers via `swHandle_t` to hide internal structures and enhance encapsulation.
- **Runtime Control**: Supports independent start, stop, and reset of timers without the need for rebuilding.
- **Dynamic Cycle Adjustment**: Allows modification of the timing cycle at runtime via pointers or APIs.
- **Static Memory Management**: All timer objects are statically allocated to prevent heap fragmentation and memory leaks.
- **Synchronization mechanism optional**: Supports SW_LoopAndTickSynchronization mode to prevent contention between the main loop and interrupt ticks.
- **Error Feedback Mechanism**: Provide detailed error codes (such as full, invalid handle, etc.) to facilitate debugging.
- **MIT Protocol Open Source**: Free to use, compatible with commercial projects.

---

🚀 快速使用
-------

### 1. 添加文件

将以下文件加入你的工程：

```tex
swTimer.c
swTimer.h
```



### 2. 定义并注册定时器

```c

// 全局变量
static unsigned int tick_10ms = 10;    // 10ms定时

// 测试用回调函数
void timer_callback_10ms(void* userData) {
    printf("[10ms] Timer triggered! Data: %d\n", *(int*)userData);
}

// 声明一个定时器实例
swTimer_t timer1 = {
    .handle = NULL,
    .cb = timer_callback_10ms,
    .user_data = &(int) { 10 },
    .status = {
        .pTicks = &tick_10ms,
        .ticks = 0,
        .periodic = SWTIMER_PERIODIC,
        .runStatus = SWTIMER_RUN
    }
};

// 创建定时器：10ms 周期
// 初始化定时器
if (swTimer_createTimer(&timer1) != SWTIMER_OK) {
    printf("Failed to create timer1\n");
    return;
}
```



### 3. 启动与控制

```c
swTimer_setRunStatus(&timer1, SWTIMER_RUN); // start timer
swTimer_setPticks(&timer1, &uintVar); // change the time limit
swTimer_setPeriodic(&timer1, SWTIMER_ONCE);
swTimer_reset(&timer1);
```



### 4. 定时触发：tick 更新

确保系统定时器中断（如 SysTick）中调用：

```c
void SysTick_Handler(void) {
    swTimer_updateTick();  // 每次中断递增内部计数器
}
```



### 5. 主循环中处理到期事件

```c
while (1) {
    swTimer_mainLoop();  // 检查所有定时器是否到期，触发回调
}
```

---

⚙️ 配置选项
-------

通过修改 swTimer.h 中的宏来自定义行为：

```c
#define TIMER_MAX_COUNT       8     // 最大支持的定时器数量
#define SW_LoopAndTickSynchronization  1  // 启用主循环与tick同步机制
```



---

## 🔄 与 MultiTimer 的对比

| 特性     | MultiTimer | swTimer       |
| ------ | ---------- | ------------- |
| 核心代码行数 | ~66 行      | ~300 行（功能更丰富） |
| 接口数量   | 4 个        | 略多，支持更细粒度控制   |
| 内存分配   | 静态         | 静态            |
| 句柄抽象   | 直接操作结构体    | 支持 handle 封装  |
| 动态周期调整 | 不支持        | 支持            |
| 运行时启停  | 不支持        | 支持            |
| 同步机制   | 无          | 可选同步          |
| 错误处理   | 简单         | 完善错误码         |
| 移植难度   | 极低         | 低             |





---

本readme基于`tongyiAI`生成，并由作者订正了内容正确性。[^1]



[^1]: [千问-Qwen最新模型体验-通义千问](https://www.qianwen.com/)
