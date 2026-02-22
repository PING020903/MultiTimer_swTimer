#include "DBG_macro.h"
#include "software_timer.h"

#ifndef I32_MAX
#define I32_MAX 0b01111111111111111111111111111111
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y))? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) > (y))? (y) : (x))
#endif

typedef struct {
    swTimerHandle_t handle;
    swTimer_t* timerInfo;
}swTimerBond_t;

static swTimerBond_t timerHandleList[TIMER_MAX_COUNT] = { 0 };

#if (SW_LoopAndTickSynchronization == true)
static volatile unsigned int syncFlag = 0U;
#endif

static bool swTimer_checkHandle(const swTimerHandle_t* current) {
    if (!current) return true;
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (timerHandleList[i].handle == *current) return false;
    }
    return true;
}

static swTimerBond_t* swTimer_getFreeTimer(void) {
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        if (timerHandleList[i].handle == SWTIMER_INIT_HANDLE ||
            timerHandleList[i].handle == SWTIMER_INVALID_HANDLE) {
            return &timerHandleList[i];
        }
    }
    return NULL;
}

static swTimerHandle_t swTimer_getNewHandle(void) {
    swTimerHandle_t maxHandleNum = SWTIMER_INVALID_HANDLE;
    for (int i = 0; i < TIMER_MAX_COUNT; i++) {
        maxHandleNum = MAX(timerHandleList[i].handle, maxHandleNum);
    }
    if (maxHandleNum <= I32_MAX) { // 尚未到达I32边界, 直接返回
        maxHandleNum++;
        return maxHandleNum;
    }

    maxHandleNum = 1; // 找空闲的handle值
    for (maxHandleNum = 1; maxHandleNum < I32_MAX; maxHandleNum++) {
        int pos;
        for (pos = 0; pos < TIMER_MAX_COUNT; pos++) {
            if (timerHandleList[pos].handle <= SWTIMER_INIT_HANDLE) continue; // 当前定时器无效, 跳过
            if (timerHandleList[pos].handle == maxHandleNum) break; // 当前handle值已被占用, 结束比较
        }
        if (pos == TIMER_MAX_COUNT) return maxHandleNum; // 当前handle值多次对比后, 无发现占用
    }

    return SWTIMER_INVALID_HANDLE;
}

static int swTimer_cfgCheck(const swTimer_t* cfg) {
    if (!cfg) return SWTIMER_ERR_ARG;

    if (swTimer_checkHandle(cfg->handle) == false) { // 该句柄非空且已被注册
        return SWTIMER_ERR_FAIL;
    }


    if (cfg->status.pTicks == NULL ||
        *(cfg->status.pTicks) == 0U) return SWTIMER_ERR_FAIL; // 未设置嘀嗒超时

    if (cfg->cb == NULL) { // 提示警告: 未注册回调函数
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
// 全局变量
static unsigned int tick_10ms = 10;    // 10ms定时
static unsigned int tick_100ms = 100;  // 100ms定时
static unsigned int tick_1s = 1000;    // 1s定时

volatile unsigned int system_ticks = 0; // 系统滴答计数

// 测试用回调函数
void timer_callback_10ms(void* userData) {
    printf("[10ms] Timer triggered! Data: %d\n", *(int*)userData);
}

void timer_callback_100ms(void* userData) {
    DEBUG_PRINT("[100ms] Timer triggered! Data: %d", *(int*)userData);
}

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

// 系统定时器回调
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
