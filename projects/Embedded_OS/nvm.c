/*
Non-volatile memory (NVM) Storage Library (25LC256)

	Author : Serge Hould
	Date: 1-3-2013

    Author: Phillip May
    Date: 3-5-2019

Notes:
 - Designed for PIC24FJ64GB002 28PIN PDIP
 - 3-5-2019 ported to PIC24FJ128GA010 and SPI2
*/

#include <p24fxxxx.h>               // PIC24 definitions

// I/O definitions
#define SPI_MASTER 0x0133           // select 8-bit master mode, CKE=1, CKP=0  SPI CLK=fcy/SPRE/PPRE=fcy/4/1 =  2MHz
#define SPI_ENABLE 0x8000           // enable SPI port, clear status

#define SCK2OUT_IO	0b01000
#define SDO2_IO		0b00111
// peripheral configurations
#define CSEE    _LATD12               // select line for Serial EEPROM
#define TCSEE   _TRISD12            // tris control for CSEE pin

// 25LC256 Serial EEPROM commands
#define SEE_WRSR    1               // write status register
#define SEE_WRITE   2               // write command
#define SEE_READ    3               // read command
#define SEE_WDI     4               // write disable
#define SEE_STAT    5               // read status register
#define SEE_WEN     6               // write enable


// initialise the Serial EEPROM
void InitNVM(void) {
    TCSEE = 0;                      // configure CSEE direction
    CSEE = 1;                       // default CSEE state
    SPI2CON1 = SPI_MASTER;          // sekect 8-bit master mode, CKE=1, CKP=0
    SPI2STAT = SPI_ENABLE;          // enable SPI port, clear status
}


// write 8-bit data to Serial EEPROM
int WriteSPI2(int data) {
    SPI2BUF = data;                 // write to buffer for TX
    while(!SPI2STATbits.SPIRBF);    // wait for the transfer to complete
    return SPI2BUF;                 // return the value received.
}

// Check the Serial EEPROM status register
int ReadSR(void) {
    int i;                          
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_STAT);            // send Read Status command
    i = WriteSPI2(0);               // send dummy, read status
    CSEE = 1;                       // deselect Serial EEPROM
    return i;                       // return status
}

// send a Write Enable command
void WriteEnable(void) {
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_WEN);             // send Write Enabled command
    CSEE = 1;                       // deselect Serial EEPROM
}

// read a 16-bit value starting at an even address
int iReadNVM(int address) {
    int lsb=0, msb=0;

    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // perform 16-bit read sequence (two byte sequential read)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_READ);            // send Read command
    WriteSPI2(address>>8);          // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)
    msb = WriteSPI2(0);             // send dummy, read data msb
    lsb = WriteSPI2(0);             // send dummy, read data lsb
    CSEE = 1;                       // deselect Serial EEPROM
    return ((msb<<8) + lsb);        // join objects + return integer
}

// write a 16-bit value of type int starting at an even address
void iWriteNVM(int address, int data) {
    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // set the write enable latch
    WriteEnable();
    
    // perform a 16-bit write sequence (2 page write)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_WRITE);           // write command
    WriteSPI2(address >> 8);        // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)
    WriteSPI2(data >> 8);           // send msb
    WriteSPI2(data & 0xff);         // send lsb
    CSEE = 1;                       // deselect the Serial EEPROM
}

// read a 32-bit value starting at an even address
long lReadNVM(int address) {
    long l_buff;
    int i, i_buff;

    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // perform 32-bit read sequence (four byte sequential read)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_READ);            // send Read command
    WriteSPI2(address>>8);          // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)

    for (i = 0; i != 4; i++) {      // repeat 4 times
        i_buff = WriteSPI2(0);      // send dummy, read data (MSB first)
        l_buff = l_buff << 8;       // shift data torward MSB
        l_buff = l_buff | i_buff;   // insert int buffer into long buffer
    }
    CSEE = 1;                       // deselect Serial EEPROM
    return l_buff;                  // join objects + return integer
}

// write a 32-bit value of type long long starting at an even address
void lWriteNVM(int address, long data) {
    int sc, i_buff;
    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // set the write enable latch
    WriteEnable();
    
    // perform a 32-bit write sequence (4 page writes)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_WRITE);           // write command
    WriteSPI2(address >> 8);        // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)

    sc = 32;                        // initialise shift count (sc)
    do {                            // repeat until all bytes are shifted out
        sc = sc - 8;                // decrement shift count (MSB first)
        i_buff = data >> sc;        // shift data (MSB first)
        WriteSPI2(i_buff & 0xFF);   // write byte to Serial EEPROM
    } while (sc != 0);              // repeat condition statement
    CSEE = 1;                       // deselect the Serial EEPROM
}

// read a 64-bit value starting at an even address
long long llReadNVM(int address) {
    long long l_buff;
    int i, i_buff;

    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // perform 64-bit read sequence (four byte sequential read)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_READ);            // send Read command
    WriteSPI2(address>>8);          // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)

    for (i = 0; i != 8; i++) {      // repeat 8 times
        i_buff = WriteSPI2(0);      // send dummy, read data (MSB first)
        l_buff = l_buff << 8;       // shift data torward MSB
        l_buff = l_buff | i_buff;   // insert int buffer into long buffer
    }
    CSEE = 1;                       // deselect Serial EEPROM
    return l_buff;                  // join objects + return integer
}

// write a 64-bit value of type long long starting at an even address
void llWriteNVM(int address, long long data) {
    int sc, i_buff;
    // wait until any work in progress is completed
    while (ReadSR() & 0x1);         // check WIP

    // set the write enable latch
    WriteEnable();
    
    // perform a 64 bit write sequence (4 page writes)
    CSEE = 0;                       // select the Serial EEPROM
    WriteSPI2(SEE_WRITE);           // write command
    WriteSPI2(address >> 8);        // address MSB first
    WriteSPI2(address & 0xfe);      // address LSB (word aligned)

    sc = 64;                        // initialise shift count (sc)
    do {                            // repeat until all bytes are shifted out
        sc = sc - 8;                // decrement shift count (MSB first)
        i_buff = data >> sc;        // shift data (MSB first)
        WriteSPI2(i_buff & 0xFF);   // write byte to Serial EEPROM
    } while (sc != 0);              // repeat condition statement
    CSEE = 1;                       // deselect the Serial EEPROM
}