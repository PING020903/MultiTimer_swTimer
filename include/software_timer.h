#ifndef _SOFTWARE_TIMER_H_
#define _SOFTWARE_TIMER_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup Software_Timer 软件定时器模块
 * @brief 提供多路软件定时器的创建、管理和调度功能
 * 
 * 该软件定时器模块支持：
 * - 多路定时器（默认 8 路，可通过 TIMER_MAX_COUNT 配置）
 * - 单次触发和周期性触发模式
 * - 回调函数机制
 * - 用户数据透传
 * 
 * @note 需要配合系统滴答定时器定期调用 swTimer_updateTick()
 * @see swTimer_createTimer(), swTimer_deleteTimer(), swTimer_mainLoop()
 * @{
 */

/**
 * @typedef swTimerHandle_t
 * @brief 定时器句柄类型，用于唯一标识一个定时器
 * @note 使用 int 类型，负值为无效句柄
 */
typedef int swTimerHandle_t;

/**
 * @typedef swTimerCB_t
 * @brief 定时器回调函数指针类型
 * @param userData 用户数据指针，由创建定时器时传入
 */
typedef void (*swTimerCB_t)(void* userData);

/**
 * @name 配置宏定义
 * @{
 */

/**
 * @def TIMER_MAX_COUNT
 * @brief 最大定时器数量（可在编译前通过预定义覆盖）
 * @note 默认值为 8
 */
#ifndef TIMER_MAX_COUNT
#define TIMER_MAX_COUNT 8  // 定时器数量
#endif

/**
 * @def SWTIMER_RUN
 * @brief 定时器运行状态标志（启用）
 */
#define SWTIMER_RUN true

/**
 * @def SWTIMER_STOP
 * @brief 定时器停止状态标志（禁用）
 */
#define SWTIMER_STOP false

/**
 * @def SWTIMER_ONCE
 * @brief 单次触发模式，定时器到期后自动停止
 */
#define SWTIMER_ONCE false

/**
 * @def SWTIMER_PERIODIC
 * @brief 周期性触发模式，定时器到期后自动重载
 */
#define SWTIMER_PERIODIC true

/**
 * @def SWTIMER_INVALID_HANDLE
 * @brief 无效句柄值，表示定时器未创建或已删除
 */
#define SWTIMER_INVALID_HANDLE -1

/**
 * @def SWTIMER_INIT_HANDLE
 * @brief 初始化句柄值，表示定时器槽位空闲
 */
#define SWTIMER_INIT_HANDLE 0

/**
 * @name 返回值错误码
 * @{
 */

/** @def SWTIMER_OK 操作成功 */
#define SWTIMER_OK 0

/** @def SWTIMER_ERR_FAIL 通用错误 */
#define SWTIMER_ERR_FAIL -1

/** @def SWTIMER_ERR_ARG 参数错误 */
#define SWTIMER_ERR_ARG -2

/** @def SWTIMER_ERR_MEM 内存不足（定时器已满） */
#define SWTIMER_ERR_MEM -3

/** @def SWTIMER_ERR_NOTSTART 定时器未启动 */
#define SWTIMER_ERR_NOTSTART -4
/** @} */

/**
 * @def SW_LoopAndTickSynchronization
 * @brief 是否启用主循环与滴答同步机制
 * @note 启用后，必须在每次调用 swTimer_mainLoop() 后才能更新滴答计数
 */
#define SW_LoopAndTickSynchronization true

/**
 * @def SWTIMER_TEST_WINDOWS
 * @brief 是否启用 Windows 平台测试代码
 * @note 设为 1 时包含 Windows 特定的测试实现
 */
#define SWTIMER_TEST_WINDOWS 1

/** @} */

/**
 * @struct swTimerStatus_t
 * @brief 定时器状态结构体，保存定时器的运行时状态信息
 * 
 * 该结构体包含定时器的核心状态信息，包括超时配置、当前计数、工作模式等。
 */
typedef struct {
    const unsigned int* pTicks;   ///< [in] 超时嘀嗒数指针（必须 ≥1），指向外部定义的超时值
    volatile unsigned int ticks;  ///< [out] 当前嘀嗒计数器，随 swTimer_updateTick() 递减
    bool periodic;                ///< [in] 定时器模式：true=周期性，false=单次触发
    bool runStatus;               ///< [in/out] 运行状态：SWTIMER_RUN 或 SWTIMER_STOP
} swTimerStatus_t;

/**
 * @struct swTimer_t
 * @brief 定时器配置结构体，用于创建和管理定时器
 * 
 * 该结构体是定时器的完整配置描述，包含句柄、回调函数、用户数据和状态信息。
 * 使用时需先填充该结构体，然后传递给 swTimer_createTimer() 进行创建。
 * 
 * @note handle 在创建时由系统自动分配，其他字段需用户初始化
 * @warning pTicks 必须指向有效的内存地址，且在定时器生命周期内保持不变
 * 
 * @see swTimer_createTimer(), swTimer_deleteTimer()
 */
typedef struct {
    const swTimerHandle_t* handle;    ///< [out] 定时器句柄指针，创建后由系统填充
    const swTimerCB_t cb;             ///< [in] 到期回调函数指针，在 swTimer_mainLoop() 中同步调用
    const void* user_data;            ///< [in] 用户数据指针，透传给回调函数（如事件 ID、上下文指针等）
    swTimerStatus_t status;           ///< [in/out] 定时器状态信息
} swTimer_t;

/**
 * @def swTimer_setPeriodic(_pCfg, _STATUS)
 * @brief 设置定时器的工作模式（单次/周期性）
 * @param _pCfg 定时器配置结构体指针
 * @param _STATUS 模式标志：SWTIMER_PERIODIC(周期) 或 SWTIMER_ONCE(单次)
 * 
 * @par 示例:
 * @code
 * swTimer_t timer;
 * swTimer_setPeriodic(&timer, SWTIMER_PERIODIC);
 * @endcode
 */
#define swTimer_setPeriodic(_pCfg, _STATUS) \
    do{\
        _pCfg->status.periodic = _STATUS;\
    }while(0)

/**
 * @def swTimer_setPticks(_pCfg, _p)
 * @brief 设置定时器的超时嘀嗒数指针
 * @param _pCfg 定时器配置结构体指针
 * @param _p 指向超时嘀嗒数的指针（必须 ≥1）
 * 
 * @note 该指针必须在定时器生命周期内保持有效
 * 
 * @par 示例:
 * @code
 * unsigned int timeout = 100;
 * swTimer_t timer;
 * swTimer_setPticks(&timer, &timeout);
 * @endcode
 */
#define swTimer_setPticks(_pCfg, _p)\
    do{\
        _pCfg->status.pTicks = _p;\
    }while(0)

/**
 * @def swTimer_setRunStatus(_pCfg, _STATUS)
 * @brief 设置定时器的运行状态（启动/停止）
 * @param _pCfg 定时器配置结构体指针
 * @param _STATUS 运行状态：SWTIMER_RUN(运行) 或 SWTIMER_STOP(停止)
 * 
 * @par 示例:
 * @code
 * swTimer_t timer;
 * swTimer_setRunStatus(&timer, SWTIMER_RUN);
 * @endcode
 */
#define swTimer_setRunStatus(_pCfg, _STATUS) \
    do{\
        _pCfg->status.runStatus = _STATUS;\
    }while(0)

/**
 * @brief 创建并注册一个新的定时器
 * 
 * 根据传入的配置结构体创建定时器，分配唯一的句柄，并将其注册到定时器列表中。
 * 创建成功后，句柄会被自动填充到 cfg->handle 中。
 * 
 * @param[in,out] cfg 定时器配置结构体指针
 *                    - handle: 输出参数，创建成功后填充
 *                    - cb: 回调函数指针（可为 NULL）
 *                    - user_data: 用户数据指针
 *                    - status.pTicks: 必须指向有效的超时嘀嗒数（≥1）
 *                    - status.periodic: 定时器模式
 *                    - status.runStatus: 初始运行状态
 * 
 * @return 返回状态码：
 *         - @ref SWTIMER_OK : 创建成功
 *         - @ref SWTIMER_ERR_ARG : 参数为空或无效
 *         - @ref SWTIMER_ERR_FAIL : 句柄已注册或配置错误
 *         - @ref SWTIMER_ERR_MEM : 定时器数量已达上限
 * 
 * @note 句柄在创建前应为 NULL 或未定义值
 * @warning 回调函数在中断上下文中执行时应避免耗时操作
 * 
 * @par 示例:
 * @code
 * unsigned int tick = 100;
 * swTimer_t timer = {
 *     .handle = NULL,
 *     .cb = my_callback,
 *     .user_data = NULL,
 *     .status = {
 *         .pTicks = &tick,
 *         .ticks = 0,
 *         .periodic = SWTIMER_PERIODIC,
 *         .runStatus = SWTIMER_RUN
 *     }
 * };
 * 
 * if (swTimer_createTimer(&timer) == SWTIMER_OK) {
 *     printf("Timer created, handle: %d\n", *timer.handle);
 * }
 * @endcode
 * 
 * @see swTimer_deleteTimer()
 */
int swTimer_createTimer(swTimer_t* cfg);

/**
 * @brief 删除指定的定时器
 * 
 * 根据句柄删除已注册的定时器，释放其占用的资源。
 * 删除后句柄变为无效，不能再用于任何操作。
 * 
 * @param[in] targetHandle 要删除的定时器句柄指针
 * 
 * @return 返回状态码：
 *         - @ref SWTIMER_OK : 删除成功
 *         - @ref SWTIMER_ERR_ARG : 句柄指针为空
 *         - @ref SWTIMER_ERR_FAIL : 未找到对应的定时器
 * 
 * @note 可以安全地多次删除同一个句柄（第一次删除后后续调用返回错误）
 * @warning 不要在回调函数中删除自身定时器，可能导致未定义行为
 * 
 * @par 示例:
 * @code
 * if (swTimer_deleteTimer(timer.handle) == SWTIMER_OK) {
 *     printf("Timer deleted\n");
 * }
 * @endcode
 * 
 * @see swTimer_createTimer()
 */
int swTimer_deleteTimer(const swTimerHandle_t* targetHandle);

/**
 * @brief 更新所有定时器的嘀嗒计数器
 * 
 * 该函数应在系统滴答中断或定时任务中被周期性调用（如每 1ms）。
 * 调用此函数会使所有运行中的定时器的 ticks 值减 1。
 * 
 * @note 当 SW_LoopAndTickSynchronization 为 true 时：
 *       - 必须在每次调用 swTimer_mainLoop() 后才能调用本函数
 *       - 确保滴答更新与主循环处理同步
 * 
 * @warning 调用频率应与 pTicks 的单位匹配（如 pTicks=10 表示 10 个调用周期）
 * 
 * @par 示例（系统滴答中断服务程序）:
 * @code
 * void SysTick_Handler(void) {
 *     swTimer_updateTick();
 * }
 * @endcode
 * 
 * @see swTimer_mainLoop()
 */
void swTimer_updateTick(void);

/**
 * @brief 软件定时器主循环处理函数
 * 
 * 该函数检查所有定时器的状态，当某个定时器的 ticks 减到 0 时：
 * 1. 调用该定时器的回调函数（如果已注册）
 * 2. 周期性定时器：重置 ticks 继续运行
 * 3. 单次定时器：停止运行
 * 
 * 该函数需要在主循环或调度器中周期性调用。
 * 
 * @return 固定返回 @ref SWTIMER_OK
 * 
 * @note 函数内部会遍历所有定时器，调用频率应高于最快定时器的频率
 * @warning 回调函数在此函数中同步执行，避免在回调中执行耗时操作
 * 
 * @par 示例（主循环）:
 * @code
 * while (1) {
 *     swTimer_mainLoop();
 *     // 其他任务...
 * }
 * @endcode
 * 
 * @see swTimer_updateTick(), swTimer_createTimer()
 */
int swTimer_mainLoop(void);

/** @} */ // end of Doxygen group

#endif // _SOFTWARE_TIMER_H_