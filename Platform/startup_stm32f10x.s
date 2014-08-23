; 栈空间意义不大，SmartOS将会重新设定到RAM最大值，这里分配的栈空间仅用于TSys构造函数重新指定栈之前
Stack_Size      EQU     0x00000400

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp

; 因为SmartOS需要大量分配内存，这里设定一个较大的值，在内存充足时尽可能分配到最大。堆空间不足时malloc将引发异常
Heap_Size       EQU     0x00004000

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
__heap_base
Heap_Mem        SPACE   Heap_Size
__heap_limit

                PRESERVE8
                THUMB


; Vector Table Mapped to Address 0 at Reset
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp  ; Top of Stack
                DCD     Reset_Handler ; Reset Handler

				IMPORT FaultHandler
				IMPORT UserHandler
                DCD     FaultHandler ; NMI Handler
                DCD     FaultHandler ; Hard Fault Handler
                DCD     FaultHandler ; MPU Fault Handler
                DCD     FaultHandler ; Bus Fault Handler
                DCD     FaultHandler ; Usage Fault Handler

				IF :DEF:GD32
                DCD     0            ; Reserved
                DCD     0            ; Reserved
                DCD     0            ; Reserved
                DCD     0            ; Reserved
                DCD     UserHandler ; SVCall Handler
                DCD     UserHandler ; Debug Monitor Handler
                DCD     0           ; Reserved
                DCD     UserHandler ; PendSV Handler
                DCD     UserHandler ; SysTick Handler

                ; External Interrupts
                DCD     UserHandler ; Window Watchdog
                DCD     UserHandler ; PVD through EXTI Line detect
                DCD     UserHandler ; Tamper
                DCD     UserHandler ; RTC
                DCD     UserHandler ; Flash
                DCD     UserHandler ; RCC
                DCD     UserHandler ; EXTI Line 0
                DCD     UserHandler ; EXTI Line 1
                DCD     UserHandler ; EXTI Line 2
                DCD     UserHandler ; EXTI Line 3
                DCD     UserHandler ; EXTI Line 4
                DCD     UserHandler ; DMA1 Channel 1
                DCD     UserHandler ; DMA1 Channel 2
                DCD     UserHandler ; DMA1 Channel 3
                DCD     UserHandler ; DMA1 Channel 4
                DCD     UserHandler ; DMA1 Channel 5
                DCD     UserHandler ; DMA1 Channel 6
                DCD     UserHandler ; DMA1 Channel 7
                DCD     UserHandler ; ADC1 & ADC2
                DCD     UserHandler ; USB High Priority or CAN1 TX
                DCD     UserHandler ; USB Low  Priority or CAN1 RX0
                DCD     UserHandler ; CAN1 RX1
                DCD     UserHandler ; CAN1 SCE
                DCD     UserHandler ; EXTI Line 9..5
                DCD     UserHandler ; TIM1 Break
                DCD     UserHandler ; TIM1 Update
                DCD     UserHandler ; TIM1 Trigger and Commutation
                DCD     UserHandler ; TIM1 Capture Compare
                DCD     UserHandler ; TIM2
                DCD     UserHandler ; TIM3
                DCD     UserHandler ; TIM4
                DCD     UserHandler ; I2C1 Event
                DCD     UserHandler ; I2C1 Error
                DCD     UserHandler ; I2C2 Event
                DCD     UserHandler ; I2C2 Error
                DCD     UserHandler ; SPI1
                DCD     UserHandler ; SPI2
                DCD     UserHandler ; USART1
                DCD     UserHandler ; USART2
                DCD     UserHandler ; USART3
                DCD     UserHandler ; EXTI Line 15..10
                DCD     UserHandler ; RTC Alarm through EXTI Line
                DCD     UserHandler ; USB Wakeup from suspend
                DCD     UserHandler ; TIM8 Break
                DCD     UserHandler ; TIM8 Update
                DCD     UserHandler ; TIM8 Trigger and Commutation
                DCD     UserHandler ; TIM8 Capture Compare
                DCD     UserHandler ; ADC3
                DCD     UserHandler ; FSMC
                DCD     UserHandler ; SDIO
                DCD     UserHandler ; TIM5
                DCD     UserHandler ; SPI3
                DCD     UserHandler ; UART4
                DCD     UserHandler ; UART5
                DCD     UserHandler ; TIM6
                DCD     UserHandler ; TIM7
                DCD     UserHandler ; DMA2 Channel1
                DCD     UserHandler ; DMA2 Channel2
                DCD     UserHandler ; DMA2 Channel3
                DCD     UserHandler ; DMA2 Channel4 & Channel5
				ENDIF
__Vectors_End

__Vectors_Size  EQU  __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY

; Reset handler
Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  __main
                IMPORT  SystemInit
                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP

                ALIGN

                 EXPORT  __initial_sp
                 EXPORT  __heap_base
                 EXPORT  __heap_limit

                 END
