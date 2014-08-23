; 栈空间意义不大，SmartOS将会重新设定到RAM最大值，这里分配的栈空间仅用于TSys构造函数重新指定栈之前
Stack_Size      EQU     0x00000400

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp

; 因为SmartOS需要大量分配内存，这里设定一个较大的值，在内存充足时尽可能分配到最大。堆空间不足时malloc将引发异常
Heap_Size       EQU     0x00000800

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
                DCD     0           ; Reserved
                DCD     0           ; Reserved
                DCD     UserHandler ; PendSV Handler
                DCD     UserHandler ; SysTick Handler

                ; External Interrupts
                DCD     UserHandler ; Window Watchdog
                DCD     UserHandler ; PVD through EXTI Line detect
                DCD     UserHandler ; RTC through EXTI Line
                DCD     UserHandler ; FLASH
                DCD     UserHandler ; RCC
                DCD     UserHandler ; EXTI Line 0 and 1
                DCD     UserHandler ; EXTI Line 2 and 3
                DCD     UserHandler ; EXTI Line 4 to 15
                DCD     UserHandler ; TS
                DCD     UserHandler ; DMA1 Channel 1
                DCD     UserHandler ; DMA1 Channel 2 and Channel 3
                DCD     UserHandler ; DMA1 Channel 4 and Channel 5
                DCD     UserHandler ; ADC1, COMP1 and COMP2 
                DCD     UserHandler ; TIM1 Break, Update, Trigger and Commutation
                DCD     UserHandler ; TIM1 Capture Compare
                DCD     UserHandler ; TIM2
                DCD     UserHandler ; TIM3
                DCD     UserHandler ; TIM6 and DAC
                DCD     0           ; Reserved
                DCD     UserHandler ; TIM14
                DCD     UserHandler ; TIM15
                DCD     UserHandler ; TIM16
                DCD     UserHandler ; TIM17
                DCD     UserHandler ; I2C1
                DCD     UserHandler ; I2C2
                DCD     UserHandler ; SPI1
                DCD     UserHandler ; SPI2
                DCD     UserHandler ; USART1
                DCD     UserHandler ; USART2
                DCD     0           ; Reserved
                DCD     UserHandler ; CEC
                DCD     0           ; Reserved
				ENDIF
				
__Vectors_End

__Vectors_Size  EQU  __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY

; Reset handler routine
Reset_Handler    PROC
                 EXPORT  Reset_Handler                 [WEAK]
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
