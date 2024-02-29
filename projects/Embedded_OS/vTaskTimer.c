/******************************************************************************
 * File:        vTaskTimer.c
 * Description: contains functions for creating/running vTaskTimer.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Created vTaskTimer for dedicated
 *                                              timer functionality
 *   "      "       May 14 2019     v1.0.1  -   Added comments for Vending Machine Project
 *****************************************************************************/

#include <string.h>
#include <stdio.h>

/* Scheduler includes. */
#include "../../Source/include/FreeRTOS.h"
#include "../../Source/include/task.h"
#include "../../Source/include/queue.h"
#include "../../Source/include/semphr.h"
#include "include/adc.h"
#include "include/pmp_lcd.h"
#include "include/public.h"
#include "include/Tick4.h"

/******************************************************************************
********************* Private static function declarations ********************
******************************************************************************/

/******************************************************************************
 * Name:        vTaskTimer
 * Description: Runs at highest priority to provide a reliable timer for vending Machine.
 *              Also toggles a heartbeat LED at 2Hz.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void vTaskTimer( void *pvParameters )
{
    int count;      // counter variable for delay
    
    pvParameters = pvParameters ; // This is to get rid of annoying warnings
    
	for( ;; )       // infinite loop
	{   
        vTaskDelay(TIMER_DELAY_TICKS);
        vSetVM((float)TIMER_DELAY_MS/1000, TIME, 0);        // stores current runtime in vendMachine data struct from vTaskUI
        
        if (count++ >= COUNT_2HZ)   // toggles LED7 at 2Hz
        {
            _RA7 ^= 1;
            count = 0;
        }
    }
}

/******************************************************************************
*************************** Public function declarations **********************
******************************************************************************/

/******************************************************************************
 * Name:        vTaskTimer
 * Description: Calls vTaskCreate() to create vTaskPoll.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vStartTaskTimer(void)
{
     xTaskCreate(	vTaskTimer,                 /* Pointer to the function that implements the task. */
					( char * ) "vTaskTimer",    /* Text name for the task.  This is to facilitate debugging only. */
					240,                        /* Stack depth in words. */
					NULL,                       /* We are not using the task parameter. */
					TIMER_TASK_PRIORITY,        /* This task will run at specified priority. */
					NULL );                     /* We are not using the task handle. */
}