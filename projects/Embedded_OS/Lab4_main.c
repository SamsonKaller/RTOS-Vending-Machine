/*****************************************************************************
 * File:        main.c
 * Description: Main source file for lab4.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Polishing Lab3 code for use as lab4
 *                                              template.
 *   "      "       Mar 04 2019     v1.1.0  -   Added LCDInit() and implemented IDLE hook.
 *   "      "       Mar 05 2019     v1.2.0  -   Added TickInit().
 *   "      "       Mar 11 2019     v1.2.1  -   Updated function names.
 *   "      "       Mar 21 2019     v1.3.0  -   Added initADC().
 *                                          -   Changed IdleHook to toggle 1 LED
 *   "      "       Mar 25 2019     v1.4.0  -   Added initUart2_wInt()
 *   "      "       Apr 08 2019     v1.5.0  -   Added vTaskTimer
 *   "      "       Apr 09 2019     v2.0.0  -   Lab 5: WatchDog
 *   "      "       Apr 16 2019     v2.1.0  -   Implemented saving in EEPROM NVM
 *   "      "       May 14 2019     v2.1.1  -   Added comments for Vending Machine Project
 *****************************************************************************/

/* Standard includes. */
#include <stdio.h>

/* Scheduler includes. */
#include "../../Source/include/FreeRTOS.h"
#include "../../Source/include/task.h"
#include "../../Source/include/queue.h"
#include "../../Source/include/semphr.h"
#include "../../Source/include/croutine.h"

//Library includes
#include "include/pmp_lcd.h"
#include "include/adc.h"
#include "include/COMM2.h"
#include "include/public.h"
#include "include/initBoard.h"
#include "include/Tick4.h"

/* Prototypes for the standard FreeRTOS callback/hook functions implemented within this file. */
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationIdleHook(void);

/* Prototype for vTaskHog for Lab5: Watchdog */
void vTaskHog(void *pvParameters);

int main( void )
{   
    /* Lines 50-51 are implemented for Lab5: Watchdog */
    PORTA = 0;                      // Turn OFF all LEDs
    if (_WDTO == 1) PORTA = 0xFF;   // Turn ON all LEDs if Watchdog timer reset
        
    /* Initialize Oscillator, IOs, and peripherals  */
    OSCILLATOR_Initialize();    // CLK config at 16MHz, also includes #pragmas for Watchdog initialization
    initIO();                   // Pushbuttons / LEDs init
    LCDInit();                  // LCD peripheral
    initADC();                  // Analog to Digital converter, for reading potentiometer
    initUart2_wInt();           // UART serial interface with interrupt on RX
    InitNVM();                  // Non-volatile memory EEPROM

    /* Tasks creation */
    vStartTaskUI();
    vStartTaskPoll();
    vStartTaskTech();
    vStartTaskTimer();
    
    /* vTaskHog creation for Lab5: Watchdog */
    xTaskCreate(vTaskHog, (char*) "vTaskHog", 240, NULL, 1, NULL);   

	/* Finally start the scheduler. */
	vTaskStartScheduler();

	/* Will only reach here if there is insufficient heap available to start the scheduler. */
    while(1);
	return(0);
}

/******************************************************************************
 * Name:        vApplicationIdleHook
 * Description: Idle Hook function which runs when all tasks are blocked. Provides
 *              Heartbeat LED while the system has idle time.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vApplicationIdleHook( void )
{
    long cnt;
    
    ClrWdt();   // Clear Watchdog timer (Lab5)
    
    for (cnt = 0; cnt < IDLE_LOOP_COUNT; cnt++);    // crude blocking delay
    _RA0 ^= 1;      // toggle LED
}

/******************************************************************************
 * Name:        vApplicationStackOverflowHook
 * Description: Traps the system during a stack overflow 
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    pxTask = pxTask;
    pcTaskName = pcTaskName;
    
    for( ;; );
}

/******************************************************************************
 * Name:        vTaskHog
 * Description: Never blocks and runs at priority 1 (lowest). Implemented so the
 *              process doesn't get any slack time, thus not running the IdleHook function,
 *              and doesn't "kick the dog" and causes a Watchdog timer reset.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vTaskHog(void *pvParameters)
{
    pvParameters = pvParameters;
    
    /* Infinite blocking loop */
    while (1);
}