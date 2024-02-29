/******************************************************************************
 * File:        vTaskTech.c
 * Description: contains functions for creating/running vTaskTech.
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author        	Date                    Comments on this revision
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Samson Kaller    Feb 25 2019     v1.0.0  -   Polishing lab3 code for use as lab4
 *                                              template.
 *   "      "       Mar 11 2019     v1.1.0  -   Changed task name to vTaskTech.
 *   "      "       Mar 11 2019     v1.1.0  -   Moved U2RXInterrupt() here to be able
 *                                              to send directly to queue.
 *   "      "       Mar 25 2019     v1.2.0  -   Added xQueueTech, _U2XInterrupt(),
 *                                              xyPutString(), printBorder(),
 *                                              printBorder().
 *                                          -   Worked on vTaskTech()
 *   "      "       Apr 01 2019     v1.3.0  -   Worked on vTaskTech()
 *   "      "       Apr 07 2019     v1.4.0  -   Increased task stack depth to 360,
 *                                              fixed numerous bugs
 *   "      "       Apr 08 2019     v1.5.0  -   vTechTask done
 *   "      "       Apr 16 2019     v2.0.0  -   Implemented saving in EEPROM NVM
 *   "      "       May 14 2019     v2.0.1  -   Added comments for Vending Machine Project
 *****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Scheduler includes. */
#include "../../Source/include/FreeRTOS.h"
#include "../../Source/include/task.h"
#include "../../Source/include/queue.h"
#include "../../Source/include/semphr.h"
#include "include/adc.h"
#include "include/public.h"
#include "include/Tick4.h"
#include "include/COMM2.h"

// Local Queue for storing incoming characters from UART RX ISR
static xQueueHandle xQueueTech;

/******************************************************************************
************************ Private function declarations ************************
******************************************************************************/

static void xyPutString(int x, int y, char* str);
static void printBorder(void);
static void printInfo(char mode);
static void updateMode(void);
static void clearMsg(void);

/******************************************************************************
 * Name:        _U2XInterrupt
 * Description: UART2 Rx ISR.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void _ISR_NO_PSV _U2RXInterrupt(void)
{
    char cChar;

	/* Get the character and post it on the queue of Rxed characters.
	If the post causes a task to wake force a context switch as the woken task
	may have a higher priority than the task we have interrupted. */
	while( U2STAbits.URXDA )
	{
		cChar = U2RXREG;
        xQueueSendFromISR(xQueueTech, &cChar, 0);
	}

    IFS1bits.U2RXIF = 0;
}

/******************************************************************************
 * Name:        vTaskTech
 * Description: Controls Technician transactions. Actions include:
 *              - Display Stock Price & stock
 *              - Display Fridge temperature
 *              - Display last transaction time
 *              - Display current balance
 *              - Empty cash balance
 *              - Reload stock
 *              - Change price of stock
 *              - Refresh menu
 *              - Exit servicing
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void vTaskTech( void *pvParameters )
{
    /* Local Variables */
    VendingMachine_t temp;      // temporary variable for mutex-protected Vending Machine Data from vTaskUI
    
    char    rxChar,                 // stores a char received from xQueueTech
            rxBuff[SIZE_RX_BUFF],   // array to store consecutive rxChar values and build a string
            txtBuff[16],            // string buffer for output to TeraTerm
            mode = MODE_HOME,       // stores the current mode of the Tech Servicing State Machine
            lastmode = 0;           // stores the last mode of the State machine
    
    int     i = 0,          // index for rxBuff array
            j = 0,          // counter for for() loops
            startFlag = 0,  // Set on the first loop when vTaskTech is run, cleared on exit from Servicing
            errFlag = 0,    // Error flag for when an invalid command is entered
            tempCode = 0;   // stores ADC code from potentiometer for Fridge Temperature 
    
    float   tempVal = 0,    // Stores converted temperature ADC code to a float value in degrees Celsius
            newVal = 0;     // Stores new input values for Stock amount and price
    
    pvParameters = pvParameters ; // This is to get rid of annoying warnings
    
    for ( ;; )
    {
        /* Block and wait to receive a char from the xQueueTech, filled by UART Rx interrupt */
        xQueueReceive(xQueueTech, &rxChar, portMAX_DELAY);

        // if startFlag is not set, initializes the tech servicing menu interface on first loop
        if (!startFlag)
        {
            startFlag = 1;
            
            vSetVM(1, TECH_SERVICING, 0);       // sets servicing flag in VendingMachine data structure in vTaskUI
            vQueueUICtrl(SM_SERVICING);         // sets vTaskUI state machine to SM_SERVIVING state
            vTaskDelay(100/portTICK_RATE_MS);   // blocks vTaskTechServicing to allow vTaskUI to process state change
            
            puts2(CLR_SCR);     // clears the UART terminal interface
            printBorder();      // prints the border for Technician Servicing Menu Interface
            
            i = 0;              // reset rxBuff index
            rxBuff[i] = '\0';   // reset rxBuff index by storing null byte in first element
            
            rxChar = '\r';                          // 
            xQueueSend(xQueueTech, &rxChar, 0);     // sends return char to refresh menu info
        }
        else
        {
            /* Switch case for char received by xQueueTech */
            switch (rxChar)
            {
                // default case will accept any char and if it is between 0-9, A-Z, a-z, or a '.' will be stored in rxBuff array
                default:
                    
                    if ((rxChar >= (char)'0' && rxChar <= (char)'9') ||   \
                        (rxChar >= (char)'A' && rxChar <= (char)'Z') ||   \
                        (rxChar >= (char)'a' && rxChar <= (char)'z') ||
                        rxChar == (char)'.')
                    {
                        if (i < SIZE_RX_BUFF - 1)   // checks if array is full
                        {
                            rxBuff[i] = rxChar;     // store rxChar in rxBuff array
                            i++;                    // increment index
                            rxBuff[i] = '\0';       // null terminate the string
                        }
                    }
                    
                break;
                
                // case if a "backspace" char is received. Clears last char in rxBuff string and corresponding char on UART terminal interface.
                case '\b':
                    
                    if (i > 0)  // if string is not empty
                    {
                        i--;                            // decrement rxBuff array index
                        rxBuff[i] = '\0';               // replace last char with null byte
                        xyPutString(10 + i, 22, " ");   // reprint ' ' (space char) over last char to erase on UART terminal interface
                    }
                    
                break;
                
                // case if the "delete" key char is received. Clears the rxBuff string and clears input line on UART terminal interface
                case 0x7F:
                    
                    i = 0;                              // reset rxBuff array index
                    rxBuff[i] = '\0';                   // empties string by null terminating first element
                    xyPutString(10, 22, "        ");    // clears input string on UART terminal interface
                
                break;
                
                // case if the "enter" key char is received. Processes input string
                case '\r':
                
                    // if the Servicing State machine switches mode, updates the UART interface with the current mode
                    if (lastmode != mode) printInfo(mode);
                    lastmode = mode;
                    
                    // first two commands 'H' and 'R' are accessible from every mode
                    if ((rxBuff[0] == 'H' || rxBuff[0] == 'h') && mode != MODE_HOME)        // returns interface to home mode
                    {
                        mode = MODE_HOME;
                        updateMode();
                        break;
                    }
                    else if (rxBuff[0] == 'R' || rxBuff[0] == 'r')      // refreshes interface screen
                    {
                        lastmode = 0;       // changes lastmode, forcing the re-printing of mode info on next loop
                        puts2(CLR_SCR);     // clears the interface screen
                        printBorder();      // re-prints border
                        updateMode();
                        break;
                    }
                    else
                    {
                        // Accessible commands depend on which mode is currently active.
                        // Switch case for tech servicing state machine
                        switch (mode)
                        {
                            // Home mode. Provides access to other modes, general commands, and exit from tech servicing
                            case MODE_HOME:

                                // Changes state machine to Stock Price Mode
                                if (rxBuff[0] == 'P' || rxBuff[0] == 'p')
                                {
                                    mode = MODE_STOCK_PRICE;
                                    updateMode();
                                
                                }
                                // Changes state machine to Stock Load Mode
                                else if (rxBuff[0] == 'L' || rxBuff[0] == 'l')
                                {
                                    mode = MODE_STOCK_LOAD;
                                    updateMode();
                                }
                                // Display stock price and count
                                else if (rxBuff[0] == 'S' || rxBuff[0] == 's')
                                {
                                    clearMsg();

                                    temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                    xyPutString(56, 7, "Stock & Price");    
                                    xyPutString(56, 8, "-------------");

                                    for (j = 0; j < DRINK_COUNT; j++)
                                    {
                                        sprintf(txtBuff, "%s: %d units @ %3.2f$", temp.drink[j].name, temp.drink[j].stock, (double)temp.drink[j].cost);
                                        xyPutString(52, 10 + j, txtBuff);
                                    }

                                    updateMode();
                                }
                                // Display fridge temperature
                                else if (rxBuff[0] == 'T' || rxBuff[0] == 't')
                                {
                                    clearMsg();

                                    tempCode = readADC(5);                  // read ADC code from potentiometer
                                    tempVal = ADC_TO_DEG((float)tempCode);  // convert ADC code to degrees Celsius

                                    xyPutString(54, 7, "Fridge Temperature");
                                    xyPutString(54, 8, "------------------");

                                    sprintf(txtBuff, "%+05.1f Degrees Celsius", (double)tempVal);
                                    xyPutString(52, 13, txtBuff);

                                    updateMode();
                                }
                                // Display last transaction time
                                else if (rxBuff[0] == 'D' || rxBuff[0] == 'd')
                                {
                                    clearMsg();

                                    temp = vmGetVM();       // get copy of VendingMachine data struct from vTaskUI

                                    xyPutString(53, 7, "Last Transaction Time");
                                    xyPutString(53, 8, "---------------------");

                                    if ((int)temp.lastTransaction != 0)     // prints last transaction time if set
                                    {
                                        // calculate last transaction time and print
                                        sprintf(txtBuff, "%.1f seconds ago", ((double)temp.time - (double)temp.lastTransaction));
                                        xyPutString(55, 13, txtBuff);
                                    }
                                    else    // if Vending Machine has just been started, no transactions have occurred.
                                    {
                                        xyPutString(48, 13, "No transactions have occurred!");
                                    }

                                    updateMode();
                                }
                                // Display current balance and customer credit
                                else if (rxBuff[0] == 'B' || rxBuff[0] == 'b')
                                {
                                    clearMsg();

                                    temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                    xyPutString(56, 7, "Current Balance");
                                    xyPutString(56, 8, "---------------");

                                    sprintf(txtBuff, "Machine Balance: %3.2f$", (double)temp.balance);
                                    xyPutString(52, 12, txtBuff);

                                    sprintf(txtBuff, "Customer Credit: %3.2f$", (double)temp.credit);
                                    xyPutString(52, 13, txtBuff);

                                    updateMode();
                                }
                                // Empty cash balance from Vending Machine
                                else if (rxBuff[0] == 'E' || rxBuff[0] == 'e')
                                {
                                    clearMsg();

                                    temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                    xyPutString(54, 7, "Empty Cash Balance");
                                    xyPutString(54, 8, "------------------");

                                    sprintf(txtBuff, "Machine Balance: %3.2f$", (double)temp.balance);
                                    xyPutString(52, 10, txtBuff);

                                    if ((int)temp.balance != 0)     // if Vending Machine has cash
                                    {
                                        xyPutString(48, 12, "Unload cash balance from");
                                        xyPutString(48, 13, "machine. When finished, press");
                                        xyPutString(48, 14, "any key to continue.");

                                        xQueueReceive(xQueueTech, &rxChar, portMAX_DELAY);  // block and wait for user input

                                        vSetVM(0, EMPTY_BALANCE, 0);    // clear balance from VendingMachine data struct from vTaskUI

                                        temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                        // display current empty balance from VendingMachine data struct from vTaskUI
                                        sprintf(txtBuff, "Balance Reset: %3.2f$", (double)temp.balance);
                                        xyPutString(48, 17, txtBuff);
                                    }
                                    else    // if vending machine has no cash
                                    {
                                        xyPutString(54, 14, "No cash in machine!");
                                    }

                                    updateMode();
                                }
                                // exit Technician Servicing
                                else if (rxBuff[0] == 'K' || rxBuff[0] == 'k')
                                {
                                    // display GoodBye message
                                    clearMsg();
                                    xyPutString(59, 12, "Goodbye!");

                                    // wait one second
                                    vTaskDelay(1000/portTICK_RATE_MS);
                                    
                                    // clear interface
                                    puts2(CLR_SCR);
                                    xyPutString(0, 0, "");

                                    // Clear servicing flag in VendingMachine data struct from vTaskUI
                                    vSetVM(0, TECH_SERVICING, 0);
                                    
                                    // reset local variables for fresh start when vTaskTech starts again
                                    lastmode = 0;
                                    startFlag = 0;
                                    i = 0;
                                    rxBuff[i] = '\0';
                                    break;
                                }

                            break;

                            // Stock Price Mode. Changes stock price.
                            case MODE_STOCK_PRICE:

                                errFlag = 1;        // sets error flag. Cleared if a valid input is processed
                                temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                if (rxBuff[0] == '\0') break;   // if rxBuff is empty, break

                                // clears interface menu message
                                for (j = 11; j < 19; j++) xyPutString(48, j, "                              ");
                                
                                // When changing stock price, the input consists of a alphabetical char, followed by price in $ format (ex: a1.25).
                                strcpy(txtBuff, rxBuff + 1);    // copies expected numerical part of string to txtBuff (starts at rxBuff[1])
                                newVal = (float)atof(txtBuff);  // converts value to float and stores in newVal

                                // if the new value entered is valid, adjust price (ie over 0 and less than or equal to 5$).
                                // atof() function returns 0 if the input value is invalid.
                                if (newVal > 0 && newVal <= 5) 
                                {
                                    for (j = 0; j < DRINK_COUNT; j++) // checks all drink stock for character match on first letter with rxBuff[0] char
                                    {
                                        if ((rxBuff[0] == temp.drink[j].name[0]) || (rxBuff[0] == temp.drink[j].name[0] + 0x20))
                                        {
                                            errFlag = 0;    // valid input, errFlag is cleared

                                            sprintf(txtBuff, "Current %s price: %3.2f$", temp.drink[j].name, (double)temp.drink[j].cost);
                                            xyPutString(48, 11, txtBuff);

                                            sprintf(txtBuff, "New %s price: %3.2f$", temp.drink[j].name, (double)newVal);
                                            xyPutString(48, 12, txtBuff);

                                            xyPutString(48, 14, "Press Y to confirm new price");
                                            xyPutString(48, 15, "Press any other key to ignore");
                                            xyPutString(48, 16, "changes");
                                            xyPutString(48, 18, "");

                                            xQueueReceive(xQueueTech, &rxChar, portMAX_DELAY);  // blocks and waits for confirmation

                                            // if yes
                                            if (rxChar == 'Y' || rxChar == 'y')
                                            {
                                                vSetVM(newVal, UPDATE_PRICE, j);
                                                temp = vmGetVM();

                                                sprintf(txtBuff, "%s price updated: %3.2f$", temp.drink[j].name, (double)temp.drink[j].cost);
                                                xyPutString(48, 18, txtBuff);
                                            }
                                            // cancels modification if no
                                            else
                                            {
                                                temp = vmGetVM();
                                                sprintf(txtBuff, "%s price unchanged: %3.2f$", temp.drink[j].name, (double)temp.drink[j].cost);
                                                xyPutString(48, 18, txtBuff);
                                            }

                                            updateMode();
                                        }
                                    }
                                }
                                
                                // if the errFlag isn't cleared by the detection of a valid input,
                                // will display error message and example on how to use command.
                                if (errFlag)
                                {
                                    sprintf(txtBuff, "Invalid command!: %s", rxBuff);
                                    xyPutString(48, 11, txtBuff);

                                    xyPutString(48, 13, "Please enter a valid KEY and");
                                    xyPutString(48, 14, "price above 0 up to a maximum");
                                    xyPutString(48, 15, "of 5 dollars");
                                    xyPutString(48, 17, "Ex: a2.50");

                                    updateMode();
                                }

                            break;

                            // Stock Load Mode. Loads more bottles into machine
                            case MODE_STOCK_LOAD:

                                errFlag = 1;        // sets error flag. Cleared if a valid input is processed
                                temp = vmGetVM();   // get copy of VendingMachine data struct from vTaskUI

                                if (rxBuff[0] == '\0') break;   // if rxBuff is empty, break

                                // clears interface menu message
                                for (j = 11; j < 19; j++) xyPutString(48, j, "                              ");

                                // When changing stock amount, the input consists of a alphabetical char, followed by amount (ex: a50).
                                strcpy(txtBuff, rxBuff + 1);    // copies expected numerical part of string to txtBuff (starts at rxBuff[1])
                                newVal = atoi(txtBuff);  // converts value to int and stores in newVal
                                
                                // if the new value entered is valid, adjust price (ie over 0 and less than or equal to 99).
                                // atoi() function returns 0 if the input value is invalid.
                                if (newVal > 0 && newVal <= 99)
                                {
                                    for (j = 0; j < DRINK_COUNT; j++) // checks all drink stock for character match on first letter with rxBuff[0] char
                                    {
                                        if (rxBuff[0] == temp.drink[j].name[0] || rxBuff[0] == temp.drink[j].name[0] + 0x20)
                                        {
                                            errFlag = 0;    // valid input, errFlag is cleared

                                            sprintf(txtBuff, "Current %s stock: %d units", temp.drink[j].name, temp.drink[j].stock);
                                            xyPutString(48, 11, txtBuff);

                                            sprintf(txtBuff, "New %s stock: %d units", temp.drink[j].name, temp.drink[j].stock + (int)newVal);
                                            xyPutString(48, 12, txtBuff);

                                            xyPutString(48, 14, "Press Y to confirm new stock");
                                            xyPutString(48, 15, "Press any other key to ignore");
                                            xyPutString(48, 16, "changes");
                                            xyPutString(48, 18, "");

                                            xQueueReceive(xQueueTech, &rxChar, portMAX_DELAY);  // blocks and waits for confirmation

                                            // if yes, updates stock 
                                            if (rxChar == 'Y' || rxChar == 'y')
                                            {
                                                vSetVM(newVal, UPDATE_STOCK, j);
                                                temp = vmGetVM();

                                                sprintf(txtBuff, "%s stock updated: %d units", temp.drink[j].name, temp.drink[j].stock);
                                                xyPutString(48, 18, txtBuff);
                                            }
                                            // if no, cancels modifications
                                            else
                                            {
                                                temp = vmGetVM();
                                                sprintf(txtBuff, "%s stock unchanged: %d units", temp.drink[j].name, temp.drink[j].stock);
                                                xyPutString(48, 18, txtBuff);
                                            }

                                            updateMode();
                                        }
                                    }
                                }

                                // if the errFlag isn't cleared by the detection of a valid input,
                                // will display error message and example on how to use command.
                                if (errFlag)
                                {
                                    sprintf(txtBuff, "Invalid command!: %s", rxBuff);
                                    xyPutString(48, 11, txtBuff);

                                    xyPutString(48, 13, "Please enter a valid KEY and");
                                    xyPutString(48, 14, "stock above 0 up to a maximum");
                                    xyPutString(48, 15, "of 99 units");
                                    xyPutString(48, 17, "Ex: a50");

                                    updateMode();
                                }
                            break;                        
                        }
                    }
                    
                break;
            }
            
            if (startFlag != 0) xyPutString(10, 22, rxBuff);
        }
        vSaveEEPROM();  // Saves VendingMachine data from vTaskUI in NVM
    }
}

/******************************************************************************
 * Name:        xyPutString
 * Description: Places a string pointed to by "str" at position (x,y) on terminal screen
 *  Parameters: - int x:        screen x coordinate
 *              - int y:        screen y coordinate
 *              - char* str:    pointer to string
 *  Return:     None
 *****************************************************************************/
static void xyPutString(int x, int y, char* str)
{
    // buffer for 
    char txtBuff[8];
    
    // VT100 Escape code character "\033" followed by move cursor command code "[Y;XH"
    sprintf(txtBuff, "\033[%d;%dH", y, x);
    puts2(txtBuff);         // print move cursor code to terminal
    puts2(str);             // puts string pointed to by str
}

/******************************************************************************
 * Name:        printBorder
 * Description: During initialization of Tech Servicing interface and when "Reset"
 *              command is issued, prints a border of '*' and '_' on terminal interface.
 *  Parameters: None 
 *  Return:     None
 *****************************************************************************/
static void printBorder(void)
{
    int i;
    
    // horizontal lines
    xyPutString(0, 0, "********************************************************************************");
    xyPutString(0, 5, "********************************************************************************");
    xyPutString(0, 8, "____________________________________________");
    xyPutString(0, 20, "********************************************************************************");
    xyPutString(0, 24, "********************************************************************************");

    // vertical lines
    for (i = 0; i < 24; i++)
    {
        xyPutString(0, i, "*");
        if (i > 5 && i < 20) xyPutString(9, i, "|");
        if (i > 5 && i < 20) xyPutString(45, i, "|");
        xyPutString(80, i, "*");
    }
}

/******************************************************************************
 * Name:        printInfo
 * Description: Prints Tech Servicing info on current mode operation. Modes are:
 *              - Home Mode
 *              - Stock Price Mode
 *              - Stock Load Mode
 *  Parameters: - char mode:    State variable for printInfo state machine
 *  Return:     None
 *****************************************************************************/
static void printInfo(char mode)
{
    int i;                      // counter variable for for() loops
    char txtBuff[8];            // string buffer to print to terminal
    VendingMachine_t temp;      // temporary variable for mutex-protected Vending Machine Data from vTaskUI
    
    xyPutString(3, 3, "Explorer 16/32 Vending Machine Service App - by Samson Kaller");
    xyPutString(4, 7, "KEY");

    // clear mode command info on terminal
    for (i = 10; i < 19; i++)
    {
        xyPutString(4, i, "   ");
        xyPutString(12, i, "                               ");
    }
    
    // switch case for printing of current mode's info
    switch (mode)
    {
        // Home Mode prints info on home screen, includes general commands,
        // commands to change mode, and refresh/exit
        case MODE_HOME:
            
            clearMsg();
            
            xyPutString(17, 7, "Operation (Home Menu) ");
            xyPutString(59, 12, "Welcome!");
            
            xyPutString(5, 10, "P");
            xyPutString(12, 10, "Stock Price Mode");

            xyPutString(5, 11, "L");
            xyPutString(12, 11, "Stock Load Mode");

            xyPutString(5, 12, "S");
            xyPutString(12, 12, "Display Stock Level & Price");

            xyPutString(5, 13, "T");
            xyPutString(12, 13, "Display Fridge Temperature");

            xyPutString(5, 14, "D");
            xyPutString(12, 14, "Display Last Transaction Time");

            xyPutString(5, 15, "B");
            xyPutString(12, 15, "Display Current Balance");

            xyPutString(5, 16, "E");
            xyPutString(12, 16, "Empty Cash Balance");

            xyPutString(5, 17, "R");
            xyPutString(12, 17, "Refresh Menu");

            xyPutString(5, 18, "K");
            xyPutString(12, 18, "Exit Servicing");
        
        break;

        // Prints info for stock price mode
        case MODE_STOCK_PRICE:
        
            clearMsg();
            
            temp = vmGetVM();

            xyPutString(17, 7, "Operation (Price Menu)");
            
            // prints first letter and name of every type of drink in stock
            for (i = 0; i < DRINK_COUNT ; i++)
            {
                sprintf(txtBuff, "%c", temp.drink[i].name[0]);
                xyPutString(5, 10 + i, txtBuff);
                sprintf(txtBuff, "Change %s Price", temp.drink[i].name);
                xyPutString(12, 10 + i, txtBuff);
            }
            
            i++;
            xyPutString(5, 10 + i, "R");
            xyPutString(12, 10 + i, "Refresh Menu");
            
            i++;
            xyPutString(5, 10 + i, "H");
            xyPutString(12, 10 + i, "Exit to Home Menu");
            
            xyPutString(48, 7, "Enter KEY followed by price in");
            xyPutString(48, 8, "dollars (0.00) to change price");
            xyPutString(48, 9, "------------------------------");
            
        break;
        
        // Prints info for stock load mode
        case MODE_STOCK_LOAD:
            
            clearMsg();
            
            temp = vmGetVM();

            xyPutString(17, 7, "Operation (Stock Menu)");

            // prints first letter and name of every type of drink in stock            
            for (i = 0; i < DRINK_COUNT ; i++)
            {
                sprintf(txtBuff, "%c", temp.drink[i].name[0]);
                xyPutString(5, 10 + i, txtBuff);
                sprintf(txtBuff, "Add %s Stock", temp.drink[i].name);
                xyPutString(12, 10 + i, txtBuff);
            }
            
            i++;
            xyPutString(5, 10 + i, "R");
            xyPutString(12, 10 + i, "Refresh Menu");
            
            i++;
            xyPutString(5, 10 + i, "H");
            xyPutString(12, 10 + i, "Exit to Home Menu");
            
            xyPutString(48, 7, "Enter KEY followed by 2 digit");
            xyPutString(48, 8, "value (00-99) to add stock");
            xyPutString(48, 9, "-----------------------------");

        break;
    }
    
    // Places input prompt and cursor at the appropriate location on screen
    xyPutString(3, 22, "INPUT: ");
}

/******************************************************************************
 * Name:        updateMode
 * Description: Due to the nature of the state machine for the Tech Servicing task
 *              which involves a queue which blocks waiting for the UART ISR input,
 *              the addition of the delete char "0x7F" in ASCII (which clears the current
 *              string input in memory and on-screen), followed by the '\r' char to
 *              the queue allows the state machine to clear user input, and make appropriate
 *              menu interface info changes upon a state change.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void updateMode(void)
{
    char rx;        // variable to add to queue
    
    _U2RXIE = 0;    // disable UART RX interrupt (this would disrupt updateMode operation if a char is transmitted between 0x7F and '\r')
    
    rx = 0x7F;
    xQueueSendFromISR(xQueueTech, &rx, 0);  // send delete char to queue (0x7F)
    rx = '\r';
    xQueueSendFromISR(xQueueTech, &rx, 0);  // send '\r' char to queue
    
    _U2RXIE = 1;    // re-enable UART RX interrupt
}

/******************************************************************************
 * Name:        clearMsg
 * Description: Clears the menu user message part of the menu interface on terminal.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
static void clearMsg(void)
{
    int i;  // loop counter
    
    // clears screen using spaces
    for (i = 7; i < 19; i++) xyPutString(48, i, "                               ");
}

/******************************************************************************
*************************** Public function declarations **********************
******************************************************************************/

/******************************************************************************
 * Name:        vStartTaskTech
 * Description: Calls vTaskCreate() to create vTaskTech.
 *  Parameters: None
 *  Return:     None
 *****************************************************************************/
void vStartTaskTech(void)
{
    xTaskCreate(	vTaskTech,              /* Pointer to the function that implements the task. */
					( char * ) "vTaskTech", /* Text name for the task.  This is to facilitate debugging only. */
					700,                    /* Stack depth in words. */
					NULL,                   /* We are not using the task parameter. */
					TECH_TASK_PRIORITY,     /* This task will run at specified priority. */
					NULL );                 /* We are not using the task handle. */
    
    // Create xQueueTech Queue. Length is 16 chars
    xQueueTech = xQueueCreate(16, sizeof(char));
}