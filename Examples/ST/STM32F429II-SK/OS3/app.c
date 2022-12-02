/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                             (c) Copyright 2013; Micrium, Inc.; Weston, FL
*
*                   All rights reserved.  Protected by international copyright laws.
*                   Knowledge of the source code may not be used to write a similar
*                   product.  This file may only be used in accordance with a license
*                   and should not be redistributed in any way.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                       IAR Development Kits
*                                              on the
*
*                                    STM32F429II-SK KICKSTART KIT
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : YS
*                 DC
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/

#define  APP_TASK_EQ_0_ITERATION_NBR              16u

/*
*********************************************************************************************************
*                                            TYPES DEFINITIONS
*********************************************************************************************************
*/
typedef enum {
	TASK_smoke,
	TASK_action,
	TASK_process,

	TASK_N
}task_e;
typedef struct
{
   CPU_CHAR* name;
   OS_TASK_PTR func;
   OS_PRIO prio;
   CPU_STK* pStack;
   OS_TCB* pTcb;
}task_t;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/
static  void  AppTaskStart          (void     *p_arg);
static  void  AppTaskCreate         (void);
static  void  AppObjCreate          (void);

static void AppTask_smoke(void *p_arg);
static void AppTask_action(void *p_arg);
static void AppTask_process(void *p_arg);

static void Setup_Gpio(void);
static void Setup_ADC(void);

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/
/* ----------------- APPLICATION GLOBALS -------------- */
static  OS_TCB   AppTaskStartTCB;
static  CPU_STK  AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

static	OS_TCB		 Task_smoke_TCB;
static	OS_TCB		 Task_led_TCB;
static  OS_TCB       Task_process_TCB;

static OS_Q AppQ1;
static OS_Q AppQ2;

static  CPU_STK  Task_smoke_Stack[APP_CFG_TASK_START_STK_SIZE];
static  CPU_STK  Task_action_Stack[APP_CFG_TASK_START_STK_SIZE];
static  CPU_STK  Task_process_Stack[APP_CFG_TASK_START_STK_SIZE];

task_t cyclic_tasks[TASK_N] = {
	{"Task_smoke", AppTask_smoke,   0, &Task_smoke_Stack[0],  &Task_smoke_TCB},
	{"Task_action", AppTask_action,	1, &Task_action_Stack[0],  &Task_led_TCB},
	{"Task_process", AppTask_process,   2, &Task_process_Stack[0],  &Task_process_TCB},
};

/* ------------ FLOATING POINT TEST TASK -------------- */
/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/

int main(void)
{
    OS_ERR  err;

    /* Basic Init */
    RCC_DeInit();
//    SystemCoreClockUpdate();
    Setup_Gpio();
    Setup_ADC();
    /* BSP Init */
    BSP_IntDisAll();                                            /* Disable all interrupts.                              */

    CPU_Init();                      	                           /* Initialize the uC/CPU Services                       */
    Mem_Init();                                                 /* Initialize Memory Management Module                  */
    Math_Init();                                                /* Initialize Mathematical Module                       */

    OSQCreate ((OS_Q *)&AppQ1, (CPU_CHAR *)"My App Queue1",
    			(OS_MSG_QTY )10, (OS_ERR *)&err);
    OSQCreate ((OS_Q *)&AppQ2, (CPU_CHAR *)"My App Queue2",
        			(OS_MSG_QTY )10, (OS_ERR *)&err);

    /* OS Init */
    OSInit(&err);                                               /* Init uC/OS-III.                                      */

    OSTaskCreate((OS_TCB       *)&AppTaskStartTCB,              /* Create the start task                                */
                 (CPU_CHAR     *)"App Task Start",
                 (OS_TASK_PTR   )AppTaskStart,
                 (void         *)0u,
                 (OS_PRIO       )APP_CFG_TASK_START_PRIO,
                 (CPU_STK      *)&AppTaskStartStk[0u],
                 (CPU_STK_SIZE  )AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE / 10u],
                 (CPU_STK_SIZE  )APP_CFG_TASK_START_STK_SIZE,
                 (OS_MSG_QTY    )0u,
                 (OS_TICK       )0u,
                 (void         *)0u,
                 (OS_OPT        )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR       *)&err);

   OSStart(&err);   /* Start multitasking (i.e. give control to uC/OS-III). */

   (void)&err;

   return (0u);
}
/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/
static  void  AppTaskStart (void *p_arg)
{
    OS_ERR  err;

   (void)p_arg;

    BSP_Init();                                                 /* Initialize BSP functions                             */
    BSP_Tick_Init();                                            /* Initialize Tick Services.                            */

#if OS_CFG_STAT_TASK_EN > 0u
    OSStatTaskCPUUsageInit(&err);                               /* Compute CPU capacity with no task running            */
#endif

#ifdef CPU_CFG_INT_DIS_MEAS_EN
    CPU_IntDisMeasMaxCurReset();
#endif

   // BSP_LED_Off(0u);                                            /* Turn Off LEDs after initialization                   */

   APP_TRACE_DBG(("Creating Application Kernel Objects\n\r"));
   AppObjCreate();                                             /* Create Applicaiton kernel objects                    */

   APP_TRACE_DBG(("Creating Application Tasks\n\r"));
   AppTaskCreate();                                            /* Create Application tasks                             */
}

/*
*********************************************************************************************************
*                                          AppTask_smoke
*
* Description : catch smoke by MQ2 sensor
*
* Arguments   : p_arg (unused)
*
* Returns     : none
*
* Note: using pin - A0
*********************************************************************************************************
*/
static void AppTask_smoke(void *p_arg)
{
    OS_ERR  err;

    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
    	int input = ADC_GetConversionValue(ADC1);

    	OSQPost((OS_Q *)&AppQ1,
    	   		(void*)&input,
    	   		(OS_MSG_SIZE)sizeof(void *),
    	   		(OS_OPT )OS_OPT_POST_FIFO,
    	   		(OS_ERR *)&err);

    	OSTimeDlyHMSM(0u, 0u, 0u, 500u,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);

    }
}

/*
*********************************************************************************************************
*                                          AppTask_action
*
* Description : BUZZER ON, and LED blinking
*
* Arguments   : p_arg (unused)
*
* Returns     : none
*
* Note: using pin - ??
*********************************************************************************************************
*/
static void AppTask_action(void *p_arg)
{
    OS_ERR  err;
    void *p_msg;
   	OS_MSG_SIZE msg_size;
   	CPU_TS ts;

   	int current = 0;

    while (DEF_TRUE) {                                          /* Task body, always written as an infinite loop.       */
    	p_msg = OSQPend((OS_Q *)&AppQ2,
    					(OS_TICK )0,
						(OS_OPT )OS_OPT_PEND_BLOCKING,
						(OS_MSG_SIZE *)&msg_size,
						(CPU_TS *)&ts,
						(OS_ERR *)&err);

    	BSP_LED_Off(current);
    	current = *(int *)p_msg;
    	BSP_LED_On(current);
    }
}

/*
*********************************************************************************************************
*                                          AppTask_process
*
* Description : DATA process & signal to task
*
* Arguments   : p_arg (unused)
*
* Returns     : none
*
* Note: none
*********************************************************************************************************
*/
static void AppTask_process(void *p_arg)
{
	OS_ERR  err;
	void *p_msg;
	OS_MSG_SIZE msg_size;
	CPU_TS ts;

	send_string("######                                 ######                                   \n\r");
	send_string("#     # #####  ######  ####   ####     #     # #    # ##### #####  ####  #    # \n\r");
	send_string("#     # #    # #      #      #         #     # #    #   #     #   #    # ##   # \n\r");
	send_string("######  #    # #####   ####   ####     ######  #    #   #     #   #    # # #  # \n\r");
	send_string("#       #####  #           #      #    #     # #    #   #     #   #    # #  # # \n\r");
	send_string("#       #   #  #      #    # #    #    #     # #    #   #     #   #    # #   ## \n\r");
	send_string("#       #    # ######  ####   ####     ######   ####    #     #    ####  #    # \n\r");
	send_string("\n\r");
	send_string("\n\r");

	send_string("                         LED 1 -> LED 2 -> LED 3 -> LED1\n\r");
	send_string("                                      ROLLING           \n\r");
	send_string("\n\r");

    while (DEF_TRUE) {

    	p_msg = OSQPend((OS_Q *)&AppQ1,
    	    					(OS_TICK )0,
    							(OS_OPT )OS_OPT_PEND_BLOCKING,
    							(OS_MSG_SIZE *)&msg_size,
    							(CPU_TS *)&ts,
    							(OS_ERR *)&err);

    	int led = *(int *)p_msg;
    	char text[20];
    	send_string("             ");
    	sprintf(text, "%d", led);
    	send_string(text);
    	send_string("\n\r");

    	OSQPost((OS_Q *)&AppQ2,
    	        	   	(void*)&led,
    	        	   	(OS_MSG_SIZE)sizeof(void *),
    	        	   	(OS_OPT )OS_OPT_POST_FIFO,
    	        	   	(OS_ERR *)&err);
    	led = led % 3 + 1;
    	*(int *)p_msg = led;
    }
}

/*
*********************************************************************************************************
*                                          AppTaskCreate()
*
* Description : Create application tasks.
*
* Argument(s) : none
*
* Return(s)   : none
*
* Caller(s)   : AppTaskStart()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  AppTaskCreate (void)
{
   OS_ERR  err;

   u8_t idx = 0;
   task_t* pTask_Cfg;
   for(idx = 0; idx < TASK_N; idx++)
   {
      pTask_Cfg = &cyclic_tasks[idx];

      OSTaskCreate(
            pTask_Cfg->pTcb,
            pTask_Cfg->name,
            pTask_Cfg->func,
            (void         *)0u,
            pTask_Cfg->prio,
            pTask_Cfg->pStack,
            pTask_Cfg->pStack[APP_CFG_TASK_START_STK_SIZE / 10u],
            APP_CFG_TASK_START_STK_SIZE,
            (OS_MSG_QTY    )0u,
            (OS_TICK       )0u,
            (void         *)0u,
            (OS_OPT        )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
            (OS_ERR       *)&err
      );
   }
}


/*
*********************************************************************************************************
*                                          AppObjCreate()
*
* Description : Create application kernel objects tasks.
*
* Argument(s) : none
*
* Return(s)   : none
*
* Caller(s)   : AppTaskStart()
*
* Note(s)     : none.
*********************************************************************************************************
*/

static  void  AppObjCreate (void)
{

}

/*
*********************************************************************************************************
*                                          Setup_Gpio()
*
* Description : Configure LED GPIOs directly
*
* Argument(s) : none
*
* Return(s)   : none
*
* Caller(s)   : AppTaskStart()
*
* Note(s)     :
*              LED1 PB0
*              LED2 PB7
*              LED3 PB14
*
*********************************************************************************************************
*/
static void Setup_Gpio(void)
{
   GPIO_InitTypeDef led_init = {0};

   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
   RCC_AHB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
   RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); // MQ2 sensor

   led_init.GPIO_Mode   = GPIO_Mode_OUT;
   led_init.GPIO_OType  = GPIO_OType_PP;
   led_init.GPIO_Speed  = GPIO_Speed_2MHz;
   led_init.GPIO_PuPd   = GPIO_PuPd_NOPULL;
   led_init.GPIO_Pin    = GPIO_Pin_0 | GPIO_Pin_7 | GPIO_Pin_14;

   GPIO_Init(GPIOB, &led_init);

   GPIO_InitTypeDef mq2_init;
   mq2_init.GPIO_Mode   = GPIO_Mode_AF;
   mq2_init.GPIO_OType  = GPIO_OType_PP;
   mq2_init.GPIO_Speed  = GPIO_Speed_2MHz;
   mq2_init.GPIO_PuPd   = GPIO_PuPd_UP;
   mq2_init.GPIO_Pin    = GPIO_Pin_0;

   GPIO_Init(GPIOA, &mq2_init);

}

static void Setup_ADC(void) {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	ADC_InitTypeDef ADC_InitStructure;
	ADC_DeInit();
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC3;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;
	ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_56Cycles);
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_Cmd(ADC1, ENABLE);
	ADC_SoftwareStartConv(ADC1);
	ADC_ContinuousModeCmd(ADC1, ENABLE);
}
