/**
  
*/

#ifndef INITBOARD_H
#define INITBOARD_H
/**
    Section: Includes
*/
#include <stdint.h>
#include <xc.h>
/**
    Section: Macros
*/
//macros definitions
/**
    Section: Function Prototypes
*/
void OSCILLATOR_Initialize(void);
void IC_config(void);    // Config IC1 module using T2 as time base
void T2_config(void);    // Config T2 module
    //initTimer3();
void initOC(void);       // Config OC module with T2
void initUart2_wInt(void);
void initADC(void);     // Init ADC
void initIO(void);



#endif
