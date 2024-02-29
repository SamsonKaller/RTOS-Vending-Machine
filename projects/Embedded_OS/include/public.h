/******************************************************************************
 * File:        public.h
 * Description: contains public prototypes and shared macros.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Polishing lab3 code for use as lab4
 *                                              template.
 *   "      "       Mar 04 2019     v1.0.1  -   Added LED macros, vending machine &
 *                                              drink structures, and IDLE hook delay
 *                                              max count macro.
 *   "      "       Mar 05 2019     v1.0.2  -   Added SKIP_COUNT macro
 *   "      "       Mar 11 2019     v1.1.0  -   Changed vTaskPoll to vTaskPollPBs
 *                                          -   Added enums, renamed some macros,
 *                                              added vQueueUICtrl(), vSetVM(), and 
 *                                              vmGetVM().
 *   "      "       Mar 25 2019     v1.2.0  -   Added prototypes for vTaskTech
 *                                              public functions.
 *                                          -   Changed enums.
 *                                          -   Changed task priorities.
 *                                          -   Added serviceFlag to VendingMachine type.
 *   "      "       Apr 08 2019     v1.3.0  -   Added macros for vTaskTimer
 *                                          -   Added more enum macros
 *                                          -   Added lastTransaction and time variables
 *                                              to VendingMachine structure
 *                                          -   Updated function prototypes
 *   "      "       May 14 2019     v1.3.1  -   Added comments for Vending Machine Project
 *****************************************************************************/

#ifndef PUBLIC_H
#define PUBLIC_H

#include "include/initBoard.h"

/*****************************************************************************/
/*********************************** MACROS **********************************/
/*****************************************************************************/

#define 	_ISR_NO_PSV 	__attribute__((__interrupt__, no_auto_psv))

// Macros for PushButtons on Explorer16/32 board
#define S3  _RD6
#define S6  _RD7
#define S5  _RA7
#define S4  _RD13

// Macros for leds on Explorer16/32 board
#define LED3    _RA0
#define LED4    _RA1
#define LED5    _RA2
#define LED6    _RA3
#define LED7    _RA4
#define LED8    _RA5
#define LED9    _RA6
#define LED10   _RA7

#define IDLE_LOOP_COUNT     100000L                 // max value for idle loop counter in IdleHook function
#define POLL_DELAY_MS       100                     // delay in ms for vTaskDelay in vTaskPoll
#define COUNT_1S            (1000 / POLL_DELAY_MS)  // value for counter variable in vTaskPoll after 1s has elapsed
#define COUNT_3S            (3000 / POLL_DELAY_MS)  // value of counter variable after 3s has elapsed
#define COUNT_250MS         (250 / POLL_DELAY_MS)   // value of counter variable after 250ms has elapsed, used for delay on pushbutton holding

// macros for ADC conversion
#define OUT_START       -7.0F       // output start (degrees Celsius)
#define OUT_END         20.0F       // output end (degrees Celsius)
#define IN_START        0.0F        // input start 0 (min ADC code)
#define IN_END          1023.0F     // input end 1023 (max ADC code)
#define SLOPE           ((OUT_END - OUT_START) / (IN_END - IN_START))   // slope for linear conversion of data
#define ADC_TO_DEG(x)   (OUT_START + SLOPE * (x))                       // macro for conversion of ADC code to degrees

#define SIZE_RX_BUFF    8           // size of RX buffer
#define CLR_SCR         "\033[2J"   // VT100 escape code to clear terminal screen

#define DRINK_COUNT 4       // total drink count
#define START_STOCK 5       // starting drink stock

// macros used in vTaskTimer for its vTaskDelay and 2Hz counting
#define TIMER_DELAY_MS      100
#define TIMER_DELAY_TICKS   (TIMER_DELAY_MS/portTICK_RATE_MS)
#define COUNT_2HZ           (1000/2/TIMER_DELAY_MS)

// Task Priorities
#define TIMER_TASK_PRIORITY 4       // real-time clock function. needs highest priority
#define TECH_TASK_PRIORITY  3       // Tech task has priority over UI and polling functionality
#define UI_TASK_PRIORITY    2       // Higher priority than vTaskPoll so that vTaskPoll does not pre-empt it
#define POLL_TASK_PRIORITY  1       // polling task requires lowest priority

// enum for macros used in vTaskTech for tech servicing interface mode
enum{   MODE_HOME = 1, MODE_STOCK_PRICE, MODE_STOCK_LOAD, MODE_HOME_PRINT, MODE_STOCK_PRICE_PRINT, MODE_STOCK_LOAD_PRINT };

// enum for macros used in vTaskUI and vTaskPoll
enum{   SM_IDLE, SM_DISPLAY_SELECTION, SM_DISPLAY_CREDIT, SM_CYCLE,     \
        SM_ADD_QUARTER, SM_CLEAR_CREDIT, SM_MAX_CREDIT, SM_TRY_VENDING, \
        SM_VEND_SUCCESS, SM_VEND_FAIL, SM_TEMP_BAD, SM_SERVICING };

// enum for macros used in vSetVM() setter function for vendMachine modification op_type
enum{   TECH_SERVICING, CLEAR_CREDIT, ADD_QUARTER, SELL_DRINK, TRANSACTION_TIME, TIME, EMPTY_BALANCE, UPDATE_PRICE, UPDATE_STOCK };

// structure for storing drink info
typedef struct
{
    char name[16];
    float cost;
    int stock;
    
} drink_t;

// structure for storing all vending machine related data.
// Local to vTaskUI and mutex protected
typedef struct
{
    drink_t drink[DRINK_COUNT];
    float balance;
    float credit;
    float time;
    float lastTransaction;
    int servicingFlag;
    
} VendingMachine_t;

/*****************************************************************************/
/************************* SHARED FUNCTION PROTOTYPES ************************/
/*****************************************************************************/

void vStartTaskTech(void);
void vStartTaskUI(void);
void vStartTaskPoll(void);
void vStartTaskTimer(void);

void vQueueUICtrl(char c);
void vSetVM(float val, char op_type, int i);
VendingMachine_t vmGetVM();

void vSaveEEPROM(void);

#endif /* PUBLIC_H */