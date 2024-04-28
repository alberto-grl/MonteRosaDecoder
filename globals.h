#ifndef __GLOBALS_H
#define __GLOBALS_H

// allocate actual memory only when invoked from the main.c module
#ifdef IN_MAIN
 #define EXTERN
#else
 #define EXTERN extern
#endif

#include "stdint.h"

#define MAX_OUTSTRINGCOUNTER (256)



EXTERN float CWLevel, SignalAverage, OldSignalAverage, BaseNoiseLevel;
EXTERN uint8_t CWIn;
EXTERN char DecodedCWChar;
EXTERN int NCharReceived;
EXTERN int CurrentAverageDah;
EXTERN float LastPulsesRatio;
EXTERN float LastDownTime;
EXTERN uint32_t CWSampleCounter;
EXTERN float TmpMaxRxVal;
EXTERN float TmpNoiseFloor;
EXTERN float RxVal_a0, RxVal_b1, NoiseFloor_a0, NoiseFloor_b1;
EXTERN uint16_t StartSquelchCounter;
EXTERN char OutString[MAX_OUTSTRINGCOUNTER + 1] ;
EXTERN uint16_t OutStringCounter;
#endif // __GLOBALS_H
