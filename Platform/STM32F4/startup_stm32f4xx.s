;!!!全局的类对象构造函数会在SmartOS重设RAM位置之前执行，要确保它们栈充足

; 栈空间意义不大，SmartOS将会重新设定到RAM最大值，这里分配的栈空间仅用于TSys构造函数重新指定栈之前
Stack_Size      EQU     0x00001000

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp

; SmartOS将会重新设定堆到RAM最大值，但是在Sys设定堆大小之前，别的地方也可能需要用到堆，特别是类的全局对象
Heap_Size       EQU     0x00001000

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

                DCD     0           ; Reserved
                DCD     0           ; Reserved
                DCD     0           ; Reserved
                DCD     0           ; Reserved
                DCD     UserHandler ; SVCall Handler
                DCD     UserHandler ; Debug Monitor Handler
                DCD     0           ; Reserved
                DCD     PendSV_Handler ; PendSV Handler
                DCD     UserHandler ; SysTick Handler

                ; External Interrupts
                DCD     UserHandler ; Window WatchDog                                        
                DCD     UserHandler ; PVD through EXTI Line detection                        
                DCD     UserHandler ; Tamper and TimeStamps through the EXTI line            
                DCD     UserHandler ; RTC Wakeup through the EXTI line                       
                DCD     UserHandler ; FLASH                                           
                DCD     UserHandler ; RCC                                             
                DCD     UserHandler ; EXTI Line0                                             
                DCD     UserHandler ; EXTI Line1                                             
                DCD     UserHandler ; EXTI Line2                                             
                DCD     UserHandler ; EXTI Line3                                             
                DCD     UserHandler ; EXTI Line4                                             
                DCD     UserHandler ; DMA1 Stream 0                                   
                DCD     UserHandler ; DMA1 Stream 1                                   
                DCD     UserHandler ; DMA1 Stream 2                                   
                DCD     UserHandler ; DMA1 Stream 3                                   
                DCD     UserHandler ; DMA1 Stream 4                                   
                DCD     UserHandler ; DMA1 Stream 5                                   
                DCD     UserHandler ; DMA1 Stream 6                                   
                DCD     UserHandler ; ADC1, ADC2 and ADC3s                            
                DCD     UserHandler ; CAN1 TX                                                
                DCD     UserHandler ; CAN1 RX0                                               
                DCD     UserHandler ; CAN1 RX1                                               
                DCD     UserHandler ; CAN1 SCE                                               
                DCD     UserHandler ; External Line[9:5]s                                    
                DCD     UserHandler ; TIM1 Break and TIM9                   
                DCD     UserHandler ; TIM1 Update and TIM10                 
                DCD     UserHandler ; TIM1 Trigger and Commutation and TIM11
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
                DCD     UserHandler ; External Line[15:10]s                                  
                DCD     UserHandler ; RTC Alarm (A and B) through EXTI Line                  
                DCD     UserHandler ; USB OTG FS Wakeup through EXTI line                        
                DCD     UserHandler ; TIM8 Break and TIM12                  
                DCD     UserHandler ; TIM8 Update and TIM13                 
                DCD     UserHandler ; TIM8 Trigger and Commutation and TIM14
                DCD     UserHandler ; TIM8 Capture Compare                                   
                DCD     UserHandler ; DMA1 Stream7                                           
                DCD     UserHandler ; FSMC                                            
                DCD     UserHandler ; SDIO                                            
                DCD     UserHandler ; TIM5                                            
                DCD     UserHandler ; SPI3                                            
                DCD     UserHandler ; UART4                                           
                DCD     UserHandler ; UART5                                           
                DCD     UserHandler ; TIM6 and DAC1&2 underrun errors                   
                DCD     UserHandler ; TIM7                   
                DCD     UserHandler ; DMA2 Stream 0                                   
                DCD     UserHandler ; DMA2 Stream 1                                   
                DCD     UserHandler ; DMA2 Stream 2                                   
                DCD     UserHandler ; DMA2 Stream 3                                   
                DCD     UserHandler ; DMA2 Stream 4                                   
                DCD     UserHandler ; Ethernet                                        
                DCD     UserHandler ; Ethernet Wakeup through EXTI line                      
                DCD     UserHandler ; CAN2 TX                                                
                DCD     UserHandler ; CAN2 RX0                                               
                DCD     UserHandler ; CAN2 RX1                                               
                DCD     UserHandler ; CAN2 SCE                                               
                DCD     UserHandler ; USB OTG FS                                      
                DCD     UserHandler ; DMA2 Stream 5                                   
                DCD     UserHandler ; DMA2 Stream 6                                   
                DCD     UserHandler ; DMA2 Stream 7                                   
                DCD     UserHandler ; USART6                                           
                DCD     UserHandler ; I2C3 event                                             
                DCD     UserHandler ; I2C3 error                                             
                DCD     UserHandler ; USB OTG HS End Point 1 Out                      
                DCD     UserHandler ; USB OTG HS End Point 1 In                       
                DCD     UserHandler ; USB OTG HS Wakeup through EXTI                         
                DCD     UserHandler ; USB OTG HS                                      
                DCD     UserHandler ; DCMI                                            
                DCD     UserHandler ; CRYP crypto                                     
                DCD     UserHandler ; Hash and Rng
                DCD     UserHandler ; FPU
__Vectors_End

__Vectors_Size  EQU  __Vectors_End - __Vectors

                AREA    |.text|, CODE, READONLY

; 启动函数
Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
				IMPORT  SysInit
				IMPORT  __main

                LDR     R0, =SysInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP

PendSV_Handler  PROC
                EXPORT  PendSV_Handler             [WEAK]
                B       .
                ENDP

                ALIGN
                
                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                END
