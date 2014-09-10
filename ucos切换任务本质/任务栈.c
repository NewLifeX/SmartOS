

// 任务栈初始化

/*
task  	任务函数指针
p_arg 	任务函数参数指针  
ptos	栈底指针
opt 	用于扩展的参数  
*/

OS_STK *OSTaskStkInit (void (*task)(void *p_arg), void *p_arg, OS_STK *ptos, INT16U opt)
{
    OS_STK *stk;


    (void)opt;                                   /* 'opt' is not used, prevent warning                 */
    stk       = ptos;                            /* Load stack pointer                                 */
                                                 /* Registers stacked as if auto-saved on exception    */
    *(stk)    = (INT32U)0x01000000L;             /* xPSR                                               */
    *(--stk)  = (INT32U)task;                    /* Entry Point                                        */
    *(--stk)  = (INT32U)0xFFFFFFFEL;             /* R14 (LR) (init value will cause fault if ever used)*/
    *(--stk)  = (INT32U)0x12121212L;             /* R12                                                */
    *(--stk)  = (INT32U)0x03030303L;             /* R3                                                 */
    *(--stk)  = (INT32U)0x02020202L;             /* R2                                                 */
    *(--stk)  = (INT32U)0x01010101L;             /* R1                                                 */
    *(--stk)  = (INT32U)p_arg;                   /* R0 : argument                                      */
                                                 /* Remaining registers saved on process stack         */
    *(--stk)  = (INT32U)0x11111111L;             /* R11                                                */
    *(--stk)  = (INT32U)0x10101010L;             /* R10                                                */
    *(--stk)  = (INT32U)0x09090909L;             /* R9                                                 */
    *(--stk)  = (INT32U)0x08080808L;             /* R8                                                 */
    *(--stk)  = (INT32U)0x07070707L;             /* R7                                                 */
    *(--stk)  = (INT32U)0x06060606L;             /* R6                                                 */
    *(--stk)  = (INT32U)0x05050505L;             /* R5                                                 */
    *(--stk)  = (INT32U)0x04040404L;             /* R4                                                 */
    return (stk);
}



// 筛选出最高优先级任务

static  void  OS_SchedNew (void)
{
#if OS_LOWEST_PRIO <= 63                         /* See if we support up to 64 tasks                   */
    INT8U   y;


    y             = OSUnMapTbl[OSRdyGrp];
    OSPrioHighRdy = (INT8U)((y << 3) + OSUnMapTbl[OSRdyTbl[y]]);
/*#else                                            // We support up to 256 tasks                         
    INT8U   y;
    INT16U *ptbl;


    if ((OSRdyGrp & 0xFF) != 0) {
        y = OSUnMapTbl[OSRdyGrp & 0xFF];
    } else {
        y = OSUnMapTbl[(OSRdyGrp >> 8) & 0xFF] + 8;
    }
    ptbl = &OSRdyTbl[y];
    if ((*ptbl & 0xFF) != 0) {
        OSPrioHighRdy = (INT8U)((y << 4) + OSUnMapTbl[(*ptbl & 0xFF)]);
    } else {
        OSPrioHighRdy = (INT8U)((y << 4) + OSUnMapTbl[(*ptbl >> 8) & 0xFF] + 8);
    }*/
#endif
}


// 任务切换

void  OS_Sched (void)
{
    OS_CPU_SR  cpu_sr;								// 临界段 备份 PRIMASK 用

    cpu_sr = CPU_SR_Save();							// 进入临界段
	
//    if (OSIntNesting == 0) {                           /* 确保不是在中断内       		*/
//        if (OSLockNesting == 0) {                      /* 确保没有锁定不许任务切换  	*/
            OS_SchedNew();
            if (OSPrioHighRdy != OSPrioCur) {            /* 最高优先级不是自己    */
                OSTCBHighRdy = OSTCBPrioTbl[OSPrioHighRdy];
                OS_TASK_SW();                            /* 执行上下文切换           */
            } 
//        }
//    }
    CPU_SR_Restore(cpu_sr);
}


// 空闲任务  我去   不使用统计任务的时候 while（1）就能搞定
void  OS_TaskIdle (void *p_arg)
{
	while(1);
}



// 任务控制块

typedef struct os_tcb {
    OS_STK          *OSTCBStkPtr;           /* sp  栈顶                         */

#if OS_TASK_CREATE_EXT_EN > 0				/* 用于统计堆栈的数据*/
    void            *OSTCBExtPtr;           /* Pointer to user definable data for TCB extension        */
    OS_STK          *OSTCBStkBottom;        /* 栈底                          */
    INT32U           OSTCBStkSize;          /* 堆栈大小 (不是字节为单位 是压栈一次所占空间)        */
//    INT16U           OSTCBOpt;              /* 扩展用参数              */
    INT16U           OSTCBId;               /* Task ID (0..65535)                                      */
#endif

    struct os_tcb   *OSTCBNext;             /* TCB双向链表                 */
    struct os_tcb   *OSTCBPrev;            


//    INT16U           OSTCBDly;              /* sleep时间   */
    INT8U            OSTCBStat;             /* 任务状态                                    */
//    INT8U            OSTCBStatPend;         /* 任务等待事件                                        */
    INT8U            OSTCBPrio;             /* ID 优先级                           */
	
											/* 任务表位图位置 xy */
    INT8U            OSTCBX;                
    INT8U            OSTCBY;               
#if OS_LOWEST_PRIO <= 63					/* 任务表位图位置的掩码  用ram换取速度 */
    INT8U            OSTCBBitX;            
    INT8U            OSTCBBitY;             
#endif

#if OS_TASK_DEL_EN > 0
    INT8U            OSTCBDelReq;           /* 是否被删除标志         */
#endif
/*
#if OS_TASK_PROFILE_EN > 0
    INT32U           OSTCBCtxSwCtr;         // 任务切换次数                
    INT32U           OSTCBCyclesTot;        // 任务运行时间统计  
    INT32U           OSTCBCyclesStart;      // xx   
    OS_STK          *OSTCBStkBase;          // 栈底            
    INT32U           OSTCBStkUsed;          // 已使用的堆栈数                   
#endif
*/

} OS_TCB;
















