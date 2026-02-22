#ifndef _SOFTWARE_TIMER_H_
#define _SOFTWARE_TIMER_H_

#include <stdint.h>
#include <stdbool.h>

typedef int swTimerHandle_t;
typedef void (*swTimerCB_t)(void* userData);

// 配置（用户可修改）
#ifndef TIMER_MAX_COUNT
#define TIMER_MAX_COUNT 8  // 定时器数量
#endif

#define SWTIMER_RUN true
#define SWTIMER_STOP false

#define SWTIMER_ONCE false
#define SWTIMER_PERIODIC true

#define SWTIMER_INVALID_HANDLE -1
#define SWTIMER_INIT_HANDLE 0

#define SWTIMER_OK 0
#define SWTIMER_ERR_FAIL -1
#define SWTIMER_ERR_ARG -2
#define SWTIMER_ERR_MEM -3
#define SWTIMER_ERR_NOTSTART -4

#define SW_LoopAndTickSynchronization true

#define SWTIMER_TEST_WINDOWS 1

typedef struct {
    const unsigned int* pTicks; // 超时嘀嗒数（必须 ≥1）
    volatile unsigned int ticks; // 当前嘀嗒数
    bool periodic;// true=周期, false=单次
    bool runStatus;// 运行状态
}swTimerStatus_t;

typedef struct {
    const swTimerHandle_t* handle; // 句柄
    const swTimerCB_t cb;         // 到期回调（在 Timer_Tick 内同步调用）
    const void* user_data;        // 透传给回调（如事件ID、上下文指针）
    swTimerStatus_t status;
}swTimer_t;

#define swTimer_setPeriodic(_pCfg, _STATUS) \
    do{\
        _pCfg->status.periodic = _STATUS;\
    }while(0)

#define swTimer_setPticks(_pCfg, _p)\
    do{\
        _pCfg->status.pTicks = _p;\
    }while(0)

#define swTimer_setRunStatus(_pCfg, _STATUS) \
    do{\
        _pCfg->status.runStatus = _STATUS;\
    }while(0)

int swTimer_createTimer(swTimer_t* cfg);

int swTimer_deleteTimer(const swTimerHandle_t* targetHandle);

void swTimer_updateTick(void);

int swTimer_mainLoop(void);

#endif
