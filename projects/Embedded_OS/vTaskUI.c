/******************************************************************************
 * File:        vTaskUI.c
 * Description: contains functions for creating/running vTaskUI.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Polishing lab3 code for use as lab4
 *                                              template.
 *   "      "       Mar 04 2019     v1.1.0  -   Started state machine for user interface
 *   "      "       Mar 05 2019     v1.1.1  -   Continued UI state machine
 *   "      "       Mar 11 2019     v1.2.0  -   Added xQueueUI and xMutexVM,
 *                                              vQueueUICtrl(), fGetVM(), vSetVM().
 *                                          -   Changed task name to vTaskUI and
 *                                              re-structured it.
 *   "      "       Mar 25 2019     v1.3.0  -   Implemented service flag for tech
 *                                              servicing message.
 *   "      "       Apr 08 2019     v1.4.0  -   Added more operations to vSetVM()
 *                                          -   Added a parameter to vSetVM()
 *                                          -   Added time since last transaction
 *   "      "       Apr 16 2019     v2.0.0  -   Implemented saving in EEPROM NVM
 *                                          -   Created vSaveEEPROM()
 *                                          -   Created vGetEEPROM()
 *   "      "       May 14 2019     v2.0.1  -   Added comments for Vending Machine Project
 *****************************************************************************/

#include <string.h>
#include <stdio.h>

/* Scheduler includes. */
#include "../../Source/include/FreeRTOS.h"
#include "../../Source/include/task.h"
#include "../../Source/include/queue.h"
#include "../../Source/include/semphr.h"
#include "include/public.h"
#include "include/Tick4.h"
#include "include/pmp_lcd.h"
#include "include/nvm.h"

/* Static struct variable for storing all vending machine related data.
 * includes stock count as well as their name and prices, starting balance, credit,
 * current time, last transaction time, and a flag for if Tech Servicing task is running
 */
                                                // Name - Cost - Stock
static VendingMachine_t vendMachine =   {   {   
                                                { "BEER",   0,     0 },
                                                { "MILK",   0,     0 },
                                                { "ICET",   0,     0 },
                                                { "COKE",   0,     0 }
                                            },
                                            0,  // Starting balance
                                            0,  // Starting credit
                                            0,  // Time (seconds)
                                            0,  // Last Transaction time
                                            0   // Servicing Flag
                                        };

// local queue for storing char data incoming to vTaskUI
static xQueueHandle xQueueUI;

// Local mutex to protect Vending machine data stuct variable
static xSemaphoreHandle xMutexVM;

/******************************************************************************
********************* Private static function declarations ********************
******************************************************************************/

static void vGetEEPROM(void);

/******************************************************************************
 * Name:        vTaskUI
 * Description: Provides user interface for vending machine on LCD screen.
 *              Gets data in xQueueUI from vTaskPoll on pushbutton / potentiometer status
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void vTaskUI( void *pvParameters )
{
    pvParameters = pvParameters ; // This is to get rid of annoying warnings
    
    VendingMachine_t temp;      // temporary variable for mutex-protected Vending Machine Data from vTaskUI
    char txtBuff[20];           // string buffer for output to LCD screen
    int i = 0,                  // index for drink selection
        failFlag = 0;           // flag for if vending fails
    char state;                 // current state of vTaskUI state machine
        
    vQueueUICtrl(SM_IDLE);
    
	for( ;; )       // infinite loop
	{
        /* Block and wait to receive a char from xQueueUI, filled by user input / potentiometer */
        xQueueReceive(xQueueUI, &state, portMAX_DELAY);
        
        temp = vmGetVM();   // get copy of VendingMachine data struct
        
        // during tech servicing, servicing flag is set, defaults state to SM_SERVICING which negates all incoming
        // data from vTaskPoll
        if (temp.servicingFlag == 1) state = SM_SERVICING;
        
        // state machine for vTaskUI. Provides user interface on LCD and pushbuttons for vending machine
        switch (state)
        {
            // IDLE state occurs when no user input has been detected for over 3s
            case SM_IDLE:

                LCDL1Home();
                LCDPutString("SELECT ITEM...  ");

                LCDL2Home();
                LCDPutString("PRESS S0++      ");

            break;
            
            // displays current drink price depending on "i" value on LCD line 1
            case SM_DISPLAY_SELECTION:

                temp = vmGetVM();

                LCDL1Home();
                sprintf(txtBuff, "%s COST: %3.2f$", temp.drink[i].name, (double)temp.drink[i].cost);
                LCDPutString(txtBuff);

                // break command omitted so that state machine automatically displays Credit during selection display
                
            // displays current customer credit in machine
            case SM_DISPLAY_CREDIT:
            
                temp = vmGetVM();

                LCDL2Home();
                sprintf(txtBuff, "CREDIT: %3.2f$   ", (double)temp.credit);
                LCDPutString(txtBuff);

            break;
            
            // if customer entered too much credit, displays MAX credit message and current credit
            case SM_MAX_CREDIT:
            
                LCDL1Home();
                LCDPutString("MAX CREDIT!     ");

                vQueueUICtrl(SM_DISPLAY_CREDIT);

            break;

            // cycles through drinks by incrementing "i"
            case SM_CYCLE:

                vQueueUICtrl(SM_DISPLAY_SELECTION);
                i = (i + 1) % DRINK_COUNT;

            break;
            
            // adds the value of a quarter in dollars to the vending machine
            case SM_ADD_QUARTER:
            
                vSetVM(0.25, ADD_QUARTER, 0);
                
                // if no try vending fail has recently occurred, changes state to display customer credit on LCD line 2 
                if (!failFlag) vQueueUICtrl(SM_DISPLAY_CREDIT);
                
                // if a try vending fail has occurred, erases the error message on LCD line 1 by printing the "Select Item" message
                // along with customer credit on line 2, then clears failFlag (because customer has started adding more credit to machine)
                else
                {
                    vQueueUICtrl(SM_IDLE);
                    vQueueUICtrl(SM_DISPLAY_CREDIT);
                    failFlag = 0;
                }
                
            break;
            
            // tries vending drink selected by "i". vSetVM() will send a VEND_SUCCESS or VEND_FAIL message to queue upon success/fail
            case SM_TRY_VENDING:
            
                temp = vmGetVM();
                vSetVM(0, SELL_DRINK, i);
            
            break;
            
            // upon receiving VEND_SUCCESS message from result of vSetVM() during SELL_DRINK operation,
            // displays vending message on selected drink and credit return
            // Clears the customer credit and stores transaction time
            case SM_VEND_SUCCESS:
            
                temp = vmGetVM();
            
                LCDL1Home();
                sprintf(txtBuff, "VENDING %s", temp.drink[i].name);
                LCDPutString(txtBuff);
                LCDPutString(" ...");

                LCDL2Home();
                sprintf(txtBuff, "RETURN: %3.2f$   ", (double)temp.credit);
                LCDPutString(txtBuff);
            
                vSetVM(0, CLEAR_CREDIT, 0);
                vSetVM(0, TRANSACTION_TIME, 0);
                
            break;
            
            // upon receiving VEND_FAIL message from result of vSetVM() during SELL_DRINK operation,
            // displays error message depending if error cause is no stock or insufficient customer credit
            case SM_VEND_FAIL:
            
                failFlag = 1;       // sets failFlag for next iteration of SM_ADD_QUARTER state. Causes that state to erase error message on line 1
                temp = vmGetVM();

                // error priority goes to customer credit first. if insufficient credit, missing credit is displayed
                if (temp.drink[i].stock != 0)
                {
                    LCDL1Home();
                    LCDPutString("MISSING CREDIT: ");

                    LCDL2Home();
                    sprintf(txtBuff, "INSERT: %3.2f$   ", (double)temp.drink[i].cost - (double)temp.credit);
                    LCDPutString(txtBuff);
                }
                
                // error message defaults to not enough of the selected drink's stock if customer has eneough credit entered.
                // displays friend apology message
                else
                {
                    LCDL1Home();
                    sprintf(txtBuff, "SORRY... %s   ", temp.drink[i].name);
                    LCDPutString(txtBuff);

                    LCDL2Home();
                    LCDPutString("OUT OF STOCK!   ");
                }
            
            break;
            
            // upon receiving a bad temperature message from vTaskPoll, displays
            // out of order message on screen
            case SM_TEMP_BAD:
            
                LCDL1Home();
                LCDPutString("OUT OF ORDER -  ");

                LCDL2Home();
                LCDPutString("TEMPERATURE FAIL");
            
            break;
            
            // when servicingFlag of VendingMachine struct is set during Tech Servicing,
            // displays out of order message on screen
            case SM_SERVICING:
            
                LCDL1Home();
                LCDPutString("OUT OF ORDER -  ");

                LCDL2Home();
                LCDPutString("TECH SERVICING  ");
                
            break;
            
            default:
            break;
        }
        
        // stores data in NVM when user input is detected
        vSaveEEPROM();
    }
}

/******************************************************************************
 * Name:        vSaveEEPROM
 * Description: Stores important info from VendingMachine data structure in NVM.
 *              Includes balance, customer credit, drink stock and cost.
 *              Dollar prices are converted into Cents in order to store integers in NVM.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vSaveEEPROM(void)
{
    int i;          // counter for loop
    int data;       // data to add to NVM
    int addr;       // address to store in NVM
    
    // directly accesses vendingMachine data securely using same Mutex as vSetVM() and vmGetVM()
    xSemaphoreTake(xMutexVM, portMAX_DELAY);
    
    // Balance stored first
    addr = 0x0000;
    data = 100 * vendMachine.balance;   // convert balance to cents
    iWriteNVM(addr, data);
    
    // Credit stored second
    addr = 0x0002;
    data = 100 * vendMachine.credit;    // convert credit to cents
    iWriteNVM(addr, data);
    
    // Drinks cost and stock stored sequentially
    for (i = 0; i < DRINK_COUNT; i++)
    {
        addr += 0x2;
        data = 100 * vendMachine.drink[i].cost;
        iWriteNVM(addr, data);
        
        addr += 0x2;
        data = vendMachine.drink[i].stock;
        iWriteNVM(addr, data);
    }
    
    xSemaphoreGive(xMutexVM);
}

/******************************************************************************
 * Name:        vGetEEPROM
 * Description: Retrieves important info from VendingMachine data structure in NVM.
 *              Includes balance, customer credit, drink stock and cost.
 *              Stored prices in cents are converted back into Dollars.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void vGetEEPROM(void)
{
    int i;          // counter for loop
    int data;       // stores data from NVM
    int addr;       // address to read in NVM
    
    // directly accesses vendingMachine data securely using same Mutex as vSetVM() and vmGetVM()
    xSemaphoreTake(xMutexVM, portMAX_DELAY);
    
    // read balance first
    addr = 0x0000;
    data = iReadNVM(addr);
    vendMachine.balance = (float)data / 100;    // convert balance to dollars and save
    
    // read credit second
    addr = 0x0002;
    data = iReadNVM(addr);
    vendMachine.credit = (float)data / 100;     // convert credit to dollars and save
    
    // Drinks cost and stock retrieved sequentially
    for (i = 0; i < DRINK_COUNT; i++)
    {
        addr += 0x2;
        data = iReadNVM(addr);
        vendMachine.drink[i].cost = (float)data / 100; //convert cost to dollars
        
        addr += 0x2;
        data = iReadNVM(addr);
        vendMachine.drink[i].stock = data;
    }
    
    xSemaphoreGive(xMutexVM);
}

/******************************************************************************
*************************** Public function declarations **********************
******************************************************************************/

/******************************************************************************
 * Name:        vStartTaskUI
 * Description: Calls vTaskCreate() to create vTaskUI.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vStartTaskUI(void)
{
     xTaskCreate(	vTaskUI,                /* Pointer to the function that implements the task. */
					( char * ) "vTaskUI",   /* Text name for the task.  This is to facilitate debugging only. */
					600,                    /* Stack depth in words. */
					NULL,                   /* We are not using the task parameter. */
					UI_TASK_PRIORITY,       /* This task will run at specified priority. */
					NULL );                 /* We are not using the task handle. */
     
     xQueueUI = xQueueCreate(4, sizeof(char));
     xMutexVM = xSemaphoreCreateMutex();
     
     vGetEEPROM();
}

/******************************************************************************
 * Name:        vQueueUICtrl
 * Description: Public function accessible by vTaskPoll to send data on pushbutton / potentiometer status
 *              to xQueueUI.
 *  Parameters: - char c:   character to add to queue. Mainly sends info from vTaskPoll on current vTaskUI state
 *  Return:     None
 *****************************************************************************/
void vQueueUICtrl(char c)
{
    // send to xQueueUI char "c" with no delay
    xQueueSend(xQueueUI, &c, 0);
}

/******************************************************************************
 * Name:        vSetVM
 * Description: Mutex protected setter function for local non-atomic data structure VendMachine,
 *              which contains all vending machine data.
 *              "val" parameter specifies a numerical value if applicable,
 *              "op_type" parameter specifies the current operation on VendMachine data struct,
 *              "i" specifies a drink if applicable
 *  Parameters: - float val:        Numerical value involved in current operation
 *              - char op_type:     The type of operation to be performed on vendMachine struct
 *              - int i:            Specifies drink
 *  Return:     None
 *****************************************************************************/
void vSetVM(float val, char op_type, int i)
{
    // vendMachine struct protected by xMutexVM
    xSemaphoreTake(xMutexVM, portMAX_DELAY);
    
    // switch case for current operation being performed on vendMachine data
    switch(op_type)
    {
        // sets the servicing flag during Technician servicing (from vTaskTech)
        case TECH_SERVICING:
            
            vendMachine.servicingFlag = (int)val;
            
        break;
        
        // clears credit after a VEND_SUCCESS
        case CLEAR_CREDIT:
        
            vendMachine.credit = 0;
        
        break;
        
        // adds a value of 25 cents to customer credit. Send MAX_CREDIT message to
        // vTaskUI if customer is trying to add more than 5 dollars to machine
        case ADD_QUARTER:
        
            if (vendMachine.credit < 5) vendMachine.credit += val;
            else vQueueUICtrl(SM_MAX_CREDIT);
            
        break;
        
        // attempts to sell a drink specified by "i". First checks customer credit, then drink stock.
        // if no errors, sends a VEND_SUCCESS message to vTaskUI, else sends a VEN_FAIL message if errors occur
        case SELL_DRINK:
        
            if (vendMachine.credit >= vendMachine.drink[i].cost)
            {
                if (vendMachine.drink[i].stock > 0)
                {
                    vendMachine.drink[i].stock -= 1;
                    vendMachine.balance += vendMachine.drink[i].cost;
                    vendMachine.credit -= vendMachine.drink[i].cost;
                
                    vQueueUICtrl(SM_VEND_SUCCESS);
                    break;                          // breaks after success to avoid sending VEND_FAIL message by default
                }
            }
        
            vQueueUICtrl(SM_VEND_FAIL);
        
        break;
        
        // logs the current time when a successful vending transaction occurs
        case TRANSACTION_TIME:
        
            vendMachine.lastTransaction = vendMachine.time;
            
        break;
        
        // vTaskTimer stores current time periodically in vendMachine struct
        case TIME:
            
            vendMachine.time += val;
            
        break;
        
        // empties balance from vendMachine (val always = 0 here)
        case EMPTY_BALANCE:
            
            vendMachine.balance = val;
            
        break;
        
        // updates the price of a drink
        case UPDATE_PRICE:
            
            vendMachine.drink[i].cost = val;
            
        break;
        
        // updates stock of a drink
        case UPDATE_STOCK:
            
            vendMachine.drink[i].stock += (int)val;
            
        break;
    }
        
    xSemaphoreGive(xMutexVM);
}

/******************************************************************************
 * Name:        vmGetVM
 * Description: Mutex protected getter function for local non-atomic data structure VendMachine,
 *              which contains all vending machine data. Returns a vendingMachin_t struct.
 *  Parameters: None
 *  Return:     - VendingMachine_t _VM:     Data from vendMachine local mutex protected structure
 *****************************************************************************/
VendingMachine_t vmGetVM()
{
    VendingMachine_t _VM;   // temp value to store vendMachine during mutex access
    
    xSemaphoreTake(xMutexVM, portMAX_DELAY);
    _VM = vendMachine;
    xSemaphoreGive(xMutexVM);
    
    return(_VM);
}