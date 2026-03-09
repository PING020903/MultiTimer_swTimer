#include "DBG_macro.h"
#include "software_timer.h"

/**
 * @file software_timer.c
 * @brief 软件定时器模块的实现文件
 * 
 * 实现了多路软件定时器的创建、管理、调度和删除功能。
 * 支持单次触发和周期性触发模式，通过回调函数机制实现定时任务。
 */

#ifndef I32_MAX
#define I32_MAX 0b01111111111111111111111111111111
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y))? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) > (y))? (y) : (x))
#endif

/**
 * @struct swTimerBond_t
 * @brief 定时器绑定结构体（内部使用）
 * 
 * 用于将句柄与定时器信息关联起来，建立句柄到定时器配置的映射关系。
 * 该结构体仅在内部管理使用，不对外暴露。
 * 
 * @see timerHandleList
 */
typedef struct {
    swTimerHandle_t handle;    ///< 定时器句柄，用于唯一标识定时器
    swTimer_t* timerInfo;      ///< 指向定时器配置结构体的指针
} swTimerBond_t;

/// @brief 全局定时器句柄列表，存储所有已创建的定时器
static swTimerBond_t timerHandleList[TIMER_MAX_COUNT] = { 0 };

#if (SW_LoopAndTickSynchronization == true)
/// @brief 同步标志位，用于确保 mainLoop 和 updateTick 的调用顺序
static volatile unsigned int syncFlag = 0U;
#endif

/**
 * @brief 检查句柄是否为空或已被注册
 * 
 * 遍历定时器列表，检查给定的句柄是否已经在系统中存在。
 * 用于防止重复注册相同的句柄。
 * 
 * @param current 待检查的句柄指针
 * 
 * @return bool 检查结果：
 *         - true: 句柄为空或未被注册（可以使用）
 *         - false: 句柄已被注册（不可使用）
 * 
 * @note 如果 current 为 NULL，直接返回 true
 * 
 * @see swTimer_cfgCheck()
 */
static bool swTimer_checkHandle(const swTimerHandle_t* current) {
    if (!current) return true;
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (timerHandleList[i].handle == *current) return false;
    }
    return true;
}

/**
 * @brief 获取一个空闲的定时器槽位
 * 
 * 遍历定时器列表，查找一个未被使用的定时器槽位。
 * 槽位空闲的判断标准是句柄为 SWTIMER_INIT_HANDLE 或 SWTIMER_INVALID_HANDLE。
 * 
 * @return swTimerBond_t* 找到的空闲定时器槽位指针
 * @retval NULL 所有定时器槽位都已占用，无可用资源
 * 
 * @note 该函数不会修改定时器状态，仅进行检查
 * 
 * @see swTimer_createTimer()
 */
static swTimerBond_t* swTimer_getFreeTimer(void) {
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (timerHandleList[i].handle == SWTIMER_INIT_HANDLE ||
            timerHandleList[i].handle == SWTIMER_INVALID_HANDLE) {
            return &timerHandleList[i];
        }
    }
    return NULL;
}

/**
 * @brief 生成一个新的唯一句柄值
 * 
 * 通过以下策略生成新的句柄：
 * 1. 首先尝试使用当前最大句柄值 + 1
 * 2. 如果达到 I32_MAX 边界，则从头搜索未使用的句柄值
 * 
 * @return swTimerHandle_t 新生成的句柄值
 * @retval SWTIMER_INVALID_HANDLE 无法找到可用的句柄值（理论上不会发生）
 * 
 * @note 句柄值从 1 开始递增，0 和负值为特殊值（无效或初始化状态）
 * @warning 在极端情况下（所有句柄值都被占用），可能返回无效句柄
 * 
 * @see swTimer_createTimer()
 */
static swTimerHandle_t swTimer_getNewHandle(void) {
    swTimerHandle_t maxHandleNum = SWTIMER_INVALID_HANDLE;
    // 查找当前最大的句柄值
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        maxHandleNum = MAX(timerHandleList[i].handle, maxHandleNum);
    }
    
    if (maxHandleNum <= I32_MAX) { // 尚未到达 I32 边界，直接递增返回
        maxHandleNum++;
        return maxHandleNum;
    }

    // 达到边界，需要查找空闲的句柄值
    maxHandleNum = 1; // 从 1 开始查找
    for (maxHandleNum = 1; maxHandleNum < I32_MAX; maxHandleNum++) {
        int pos;
        for (pos = 0; pos < TIMER_MAX_COUNT; pos++) {
            if (timerHandleList[pos].handle <= SWTIMER_INIT_HANDLE) continue; // 当前定时器无效，跳过
            if (timerHandleList[pos].handle == maxHandleNum) break; // 当前句柄值已被占用，结束比较
        }
        if (pos == TIMER_MAX_COUNT) return maxHandleNum; // 当前句柄值未被占用，可以返回
    }

    return SWTIMER_INVALID_HANDLE; // 找不到可用句柄
}

/**
 * @brief 验证定时器配置参数的合法性
 * 
 * 对传入的定时器配置进行全面检查，包括：
 * - 配置结构体指针是否为空
 * - 句柄是否已被注册
 * - 超时嘀嗒数是否有效
 * - 回调函数是否已注册（仅警告）
 * 
 * @param cfg 定时器配置结构体指针
 * 
 * @return int 检查结果：
 *         - @ref SWTIMER_OK : 配置合法
 *         - @ref SWTIMER_ERR_ARG : 参数为空
 *         - @ref SWTIMER_ERR_FAIL : 句柄已注册或 pTicks 无效
 *         - @ref SWTIMER_OK : 其他情况（即使回调未注册也返回成功）
 * 
 * @note 如果回调函数未注册，会打印警告但依然返回成功
 * @warning pTicks 必须指向有效的内存地址且值不能为 0
 * 
 * @see swTimer_createTimer()
 */
static int swTimer_cfgCheck(const swTimer_t* cfg) {
    if (!cfg) return SWTIMER_ERR_ARG;

    if (swTimer_checkHandle(cfg->handle) == false) { // 句柄已被注册
        return SWTIMER_ERR_FAIL;
    }

    // 检查超时嘀嗒数配置
    if (cfg->status.pTicks == NULL ||
        *(cfg->status.pTicks) == 0U) return SWTIMER_ERR_FAIL;

    // 检查回调函数（可选）
    if (cfg->cb == NULL) {
        DEBUG_PRINT("this callback is not register.");
    }
    return SWTIMER_OK;
}

int swTimer_createTimer(swTimer_t* cfg) {
    swTimerBond_t* currentTimer = NULL;
    if (!cfg) return SWTIMER_ERR_ARG; // 参数为空

    if (swTimer_cfgCheck(cfg) != SWTIMER_OK) return SWTIMER_ERR_FAIL; // 初始化配置设置错误

    currentTimer = swTimer_getFreeTimer();
    if (currentTimer == NULL) return SWTIMER_ERR_MEM; // 定时器已满

    currentTimer->handle = swTimer_getNewHandle();
    if (currentTimer->handle <= SWTIMER_INIT_HANDLE) return SWTIMER_ERR_FAIL; // 判断handle值是否有效
    cfg->handle = &(currentTimer->handle);
    cfg->status.ticks = *(cfg->status.pTicks);
    currentTimer->timerInfo = cfg; // 将handle与配置绑定

    return SWTIMER_OK;
}

int swTimer_deleteTimer(const swTimerHandle_t* targetHandle) {
    if (!targetHandle) return SWTIMER_ERR_ARG;

    for (int i = 0; i < TIMER_MAX_COUNT; i++) { // 寻找目标handle地址并移除该定时器
        if (&timerHandleList[i].handle == targetHandle) {
            timerHandleList[i].handle = SWTIMER_INVALID_HANDLE;
            timerHandleList[i].timerInfo = NULL;
            return SWTIMER_OK;
        }
    }
    return SWTIMER_ERR_FAIL;
}

void swTimer_updateTick(void) {
#if (SW_LoopAndTickSynchronization == true)
    if (!syncFlag) return; // 必须在执行一次mainloop后才会更新一次tick
    syncFlag = 0U;
#endif
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (timerHandleList[i].handle != SWTIMER_INVALID_HANDLE &&
            *(timerHandleList[i].timerInfo->status.pTicks) > 0 &&
            timerHandleList[i].timerInfo->status.runStatus == SWTIMER_RUN) {
            timerHandleList[i].timerInfo->status.ticks--;
        }
    }
}

// 该函数虽名为主循环, 但实际上并没有一个死循环, 而是通过外部循环不停的调用该函数
// 当定时时间抵达时, 将会自动调用被注册的定时器回调函数
int swTimer_mainLoop(void) {
    volatile unsigned int tempTicks = 0U;
#if (SW_LoopAndTickSynchronization == true)
    syncFlag = 1U;
#endif
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        // 检查定时器是否有效且正在运行
        if (timerHandleList[i].handle <= SWTIMER_INIT_HANDLE ||
            timerHandleList[i].timerInfo == NULL ||
            timerHandleList[i].timerInfo->status.runStatus == SWTIMER_STOP) {
            continue; // 检查下一个定时器
        }
        tempTicks = timerHandleList[i].timerInfo->status.ticks; // 防止在中断中被修改

        // 检查是否超时 (ticks已减到0)
        if (tempTicks == 0) {
            // 调用回调函数(如果已注册)
            if (timerHandleList[i].timerInfo->cb != NULL) {
                timerHandleList[i].timerInfo->cb((void*)timerHandleList[i].timerInfo->user_data);
            }

            // 根据定时器类型处理后续状态
            if (timerHandleList[i].timerInfo->status.periodic == SWTIMER_PERIODIC) {
                // 周期性定时器：重置计数值，继续运行
                timerHandleList[i].timerInfo->status.ticks = *(timerHandleList[i].timerInfo->status.pTicks);
            }
            else {
                // 单次定时器：停止运行
                timerHandleList[i].timerInfo->status.runStatus = SWTIMER_STOP;
            }
        }
    }
    return SWTIMER_OK;
}
/********************************************************/
#if SWTIMER_TEST_WINDOWS
#include "Windows.h"

/// @brief 10ms 定时器的超时嘀嗒数
static unsigned int tick_10ms = 10;

/// @brief 100ms 定时器的超时嘀嗒数
static unsigned int tick_100ms = 100;

/// @brief 1s 定时器的超时嘀嗒数
static unsigned int tick_1s = 1000;

/// @brief 系统滴答计数器，记录经过的毫秒数
volatile unsigned int system_ticks = 0;

/**
 * @brief 10ms 定时器的测试回调函数
 * 
 * 当 10ms 定时器超时时被调用，打印调试信息。
 * 
 * @param userData 用户数据指针，期望是 int 类型的指针
 */
void timer_callback_10ms(void* userData) {
    printf("[10ms] Timer triggered! Data: %d\n", *(int*)userData);
}

/**
 * @brief 100ms 定时器的测试回调函数
 * 
 * 当 100ms 定时器超时时被调用，打印调试信息。
 * 
 * @param userData 用户数据指针，期望是 int 类型的指针
 */
void timer_callback_100ms(void* userData) {
    DEBUG_PRINT("[100ms] Timer triggered! Data: %d", *(int*)userData);
}

/**
 * @brief 1s 定时器的测试回调函数
 * 
 * 当 1s 定时器超时时被调用，打印调试信息。
 * 
 * @param userData 用户数据指针，期望是 int 类型的指针
 */
void timer_callback_1s(void* userData) {
    DEBUG_PRINT("[1s] Timer triggered! Data: %d", *(int*)userData);
}

// 定时器配置
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

swTimer_t timer2 = {
    .handle = NULL,
    .cb = timer_callback_100ms,
    .user_data = &(int) { 100 },
    .status = {
        .pTicks = &tick_100ms,
        .ticks = 0,
        .periodic = SWTIMER_PERIODIC,
        .runStatus = SWTIMER_RUN
    }
};

swTimer_t timer3 = {
    .handle = NULL,
    .cb = timer_callback_1s,
    .user_data = &(int) { 1000 },
    .status = {
        .pTicks = &tick_1s,
        .ticks = 0,
        .periodic = SWTIMER_ONCE, // 单次定时器
        .runStatus = SWTIMER_RUN
    }
};

/**
 * @brief 系统定时器回调函数（Windows 专用）
 * 
 * 由 Windows 系统定时器触发，每 1ms 调用一次。
 * 负责更新系统滴答计数并调用软件定时器更新函数。
 * 
 * @param hwnd 窗口句柄（未使用）
 * @param uMsg 消息标识符（未使用）
 * @param idEvent 定时器 ID（未使用）
 * @param dwTime 系统时间（未使用）
 * 
 * @note 该函数在中断上下文中执行，应尽量避免耗时操作
 */
VOID CALLBACK SystemTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    system_ticks++;
    swTimer_updateTick();
}

void swTimer_test(void) {
    // 初始化定时器
    if (swTimer_createTimer(&timer1) != SWTIMER_OK) {
        printf("Failed to create timer1\n");
        return;
    }

    if (swTimer_createTimer(&timer2) != SWTIMER_OK) {
        printf("Failed to create timer2\n");
        return;
    }

    if (swTimer_createTimer(&timer3) != SWTIMER_OK) {
        printf("Failed to create timer3\n");
        return;
    }

    DEBUG_PRINT("Software timers initialized. Press Ctrl+C to exit.");
    DEBUG_PRINT("Timer1: 10ms periodic");
    DEBUG_PRINT("Timer2: 100ms periodic");
    DEBUG_PRINT("Timer3: 1000ms one-shot");

    // 设置系统定时器，每1ms触发一次
    UINT_PTR timerId = SetTimer(NULL, 0, 1, (TIMERPROC)SystemTimerProc);
    if (timerId == 0) {
        printf("Failed to create system timer\n");
        return;
    }

    // 主循环
    MSG msg;
    for (int i = 0; i < 5000; i++) {
        // 处理Windows消息
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 处理软件定时器
        swTimer_mainLoop();

        // 适当休眠，减少CPU占用
        Sleep(1);
    }

    // 清理
    KillTimer(NULL, timerId);
    VAR_PRINT_INT(swTimer_deleteTimer(timer1.handle));
    VAR_PRINT_INT(swTimer_deleteTimer(timer1.handle));
    VAR_PRINT_INT(swTimer_deleteTimer(timer2.handle));
    VAR_PRINT_INT(swTimer_deleteTimer(timer3.handle));
}
#endif
