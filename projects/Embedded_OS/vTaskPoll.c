/******************************************************************************
 * File:        vTaskPoll.c
 * Description: contains functions for creating/running vTaskPoll.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Polishing lab3 code for use as lab4
 *                                              template.
 *   "      "       Mar 11 2019     v1.1.0  -   Renamed task to vTaskPollPBs
 *   "      "       Mar 21 2019     v1.2.0  -   Renamed task to vTaskPoll
 *   "      "       May 14 2019     v1.2.1  -   Added comments for Vending Machine Project
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
 * Name:        vTaskPoll
 * Description: Polls push buttons for user input, temperature reading from pot via ADC,
 *              and causes vTaskUI to revert to idle state if no input is detected for 3s.
 *              Task unblocks according to POLL_DELAY_MS macro.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void vTaskPoll( void *pvParameters )
{    
    int     cntDelay = 0,           // counter for 3s delay
            flagS3 = 0, flagS4 = 0, // flags for pushbutton S3 and S4
            tempCode = 0;           // stores ADC code from potentiometer (temperature)
    float   tempVal = 0;            // stores converted temperature value from potentiometer
    
    pvParameters = pvParameters ; // This is to get rid of annoying warnings
    
	for( ;; )       // infinite loop
	{   
        vTaskDelay(POLL_DELAY_MS / portTICK_RATE_MS);
        
        // get temperature code from ADC and convert to degrees Celsius
        tempCode = readADC(5);
        tempVal = ADC_TO_DEG((float)tempCode);
        
        // 1st priority, check for valid temperature (also displays Out Of Stock message from vTaskUI and doesnt allow any other input)
        if (tempVal < 0 || tempVal > 8)
        {
            vQueueUICtrl(SM_TEMP_BAD);
            cntDelay = COUNT_3S;        // immediately triggers cntDelay if() statement to update LCD once temperature is good again
        }
        
        // check if cycle drinks button is pressed
        else if (!S3)
        {
            if (!flagS3)    // if no previous press is detected
            {
                vQueueUICtrl(SM_CYCLE);     // cycle drink via vTaskUI
                flagS3 = 1;                 // set S3 flag
            }
            else if (flagS3++ >= COUNT_250MS) flagS3 = 0;   // if previous press is detected
            cntDelay = 0;       // resets idle counter 3s delay
        }
        
        // check if add quarter button is pressed
        else if (!S4)
        {
            if (!flagS4)    // if no previous press is detected
            {
                vQueueUICtrl(SM_ADD_QUARTER);   // add quarter via vTaskUI
                flagS4 = 1;                     // set S4 flag
            }
            else if (flagS4++ >= COUNT_250MS) flagS4 = 0;   // if previous press detected
            cntDelay = 0;           // reset idle counter 3s delay
        }
        
        // check if try vending button is pressed
        else if (!S6)
        {
            while (!S6);                    // blocks while button is held down (to only vend once)
            vQueueUICtrl(SM_TRY_VENDING);
            cntDelay = COUNT_1S;            // reduce default 3s idle delay to 2s
        }
        
        // counts iterations of vTaskPoll, and unblocks once it has looped for 3s (according to COUNT_3S and POLL_DELAY_MS from vTaskDelay())
        else if (cntDelay++ >= COUNT_3S)
        {            
            vQueueUICtrl(SM_IDLE);
            cntDelay = 0;
        }
        
        // default else statement clears all button press flag if none detected on current loop iteration
        else
        {
            flagS3 = 0;
            flagS4 = 0;
        }
    }
}

/******************************************************************************
*************************** Public function declarations **********************
******************************************************************************/

/******************************************************************************
 * Name:        vTaskPoll
 * Description: Calls vTaskCreate() to create vTaskPoll.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vStartTaskPoll(void)
{
     xTaskCreate(	vTaskPoll,                  /* Pointer to the function that implements the task. */
					( char * ) "vTaskPoll",     /* Text name for the task.  This is to facilitate debugging only. */
					240,                        /* Stack depth in words. */
					NULL,                       /* We are not using the task parameter. */
					POLL_TASK_PRIORITY,         /* This task will run at specified priority. */
					NULL );                     /* We are not using the task handle. */
}