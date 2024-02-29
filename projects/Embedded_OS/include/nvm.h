/*

NVM Storgage Library (25LC256)

Notes:
 - Designed for PIC24 + Explorer16

*/


// intialise access to memory device
void InitNVM(void);


// 16/32/64 bit read and write functions
// NOTE: address must be an even value between 0x0000 and 0x7ffe
// (see the page write restrictions on the device datasheet)
int iReadNVM(int address);
void iWriteNVM(int address, int data);

long lReadNVM(int address);
void lWriteNVM(int address, long data);

long long llReadNVM(int address);
void llWriteNVM(int address, long long data);

