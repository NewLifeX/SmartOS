




    EXTERN  OSRunning                   ; 当前os状态
    EXTERN  OSPrioCur					; 当前任务优先级
    EXTERN  OSPrioHighRdy				; 当前最高优先级
    EXTERN  OSTCBCur					; 当前任务块
    EXTERN  OSTCBHighRdy				; 当前最高优先级的任务块
    EXTERN  OSIntNesting				; 中断嵌套层数
    EXTERN  OSIntExit
;    EXTERN  OSTaskSwHook				; 钩子函数


    EXPORT  OS_CPU_SR_Save              ; 临界段保护不被中断
    EXPORT  OS_CPU_SR_Restore			; 退出临界段 恢复中断
    EXPORT  OSStartHighRdy				; 启动OS
    EXPORT  OSCtxSw						; 触发pendsv中断
    EXPORT  OSIntCtxSw
    EXPORT  PendSV_Handler				; pendsv中断





; 中断总开关

CPU_IntDis
        CPSID   I
        BX      LR


CPU_IntEn
        CPSIE   I
        BX      LR

		
		
		
		
; 临界区使用掩码   终止用户中断发生  并备份primask
CPU_SR_Save
        MRS     R0, PRIMASK                     ; ;Set prio int mask to mask all (except faults)
        CPSID   I
        BX      LR

; 出临界代码段
CPU_SR_Restore                                  
        MSR     PRIMASK, R0
        BX      LR
		
		
		
		
		
		
		
; 启动 os  开始任务调度
OSStartHighRdy
	LDR	R0,=NVIC_SYSPRI14				
	LDR R1,=NVIC_PENDSV_PRI
	STRB R1,[R0]
	
	MRS R0,MSP
	MSR PSP,R0
	
	MRS R0,CONTROL
	ORR R0,R0,#0X02
	MSR CONTROL,R0
	
	LDR R1,=OSTCBCur
	MRS R0,PSP
	SUBS R0,R0,#0X24
	STR R0,[R1]
    ;LDR     R0, =NVIC_SYSPRI14       
    ;LDR     R1, =NVIC_PENDSV_PRI
    ;STRB    R1, [R0]

    ;MOVS    R0, #0                                              ; Set the PSP to 0 for initial context switch call
    ;MSR     PSP, R0

    LDR     R0, =OSRunning                                      ; OSRunning = TRUE
    MOVS    R1, #1
    STRB    R1, [R0]

    LDR     R0, =NVIC_INT_CTRL                                  ; Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]

    CPSIE   I                                                   ; Enable interrupts at processor level

; 如果执行到下面这一行说明启动失败  ;一般是pendsv无法响应
OSStartHang			
    B       OSStartHang                                         ; Should never get here


	
; 触发pendsv中断
OSIntCtxSw
OSCtxSw
    LDR     R0, =NVIC_INT_CTRL                                  ; Trigger the PendSV exception (causes context switch)
    LDR     R1, =NVIC_PENDSVSET
    STR     R1, [R0]
    BX      LR	
	
	
	
	
	
	
; 任务切换本质  pendsv中断本体

PendSV_Handler
    CPSID   I                                                   ; Prevent interruption during context switch
    MRS     R0, PSP                                             ; PSP is process stack pointer
    CBZ     R0, OS_CPU_PendSVHandler_nosave                     ; Skip register save the first time

    SUBS    R0, R0, #0x20                                       ; 保存现场  r4-11   r0-3 pc等都被中断时压栈过了
    STM     R0, {R4-R11}

    LDR     R1, =OSTCBCur                                       ; OSTCBCur->OSTCBStkPtr = SP; 备份当前sp到任务控制块
    LDR     R1, [R1]
    STR     R0, [R1]                                            ; R0 is SP of process being switched out

                                                                ; At this point, entire context of process has been saved
OS_CPU_PendSVHandler_nosave


; 钩子函数   备注掉
;    PUSH    {R14}                                               ; Save LR exc_return value
;    LDR     R0, =OSTaskSwHook                                   ; OSTaskSwHook();
;    BLX     R0
;    POP     {R14}
	

    LDR     R0, =OSPrioCur                                      ; OSPrioCur = OSPrioHighRdy;	当前优先级切换到当前最高优先级
    LDR     R1, =OSPrioHighRdy
    LDRB    R2, [R1]
    STRB    R2, [R0]

    LDR     R0, =OSTCBCur                                       ; OSTCBCur  = OSTCBHighRdy;		当前任务快跟随切换
    LDR     R1, =OSTCBHighRdy
    LDR     R2, [R1]
    STR     R2, [R0]

    LDR     R0, [R2]                                            ; R0 is new process SP; SP = OSTCBHighRdy->OSTCBStkPtr;
    LDM     R0, {R4-R11}                                        ; Restore r4-11 from new process stack
    ADDS    R0, R0, #0x20
    MSR     PSP, R0                                             ; Load PSP with new process SP
    ORR     LR, LR, #0x04                                       ; Ensure exception return uses process stack
    CPSIE   I
    BX      LR                                                  ; Exception return will restore remaining context

	
