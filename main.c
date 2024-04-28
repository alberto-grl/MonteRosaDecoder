/*
 * TestSCAMP.c
 *
 *  Created on: 17 Apr 2024
 *      Author: Alberto Garlassi I4NZX
 *
 *
 *
 *
 */
/*
* Copyright (c) 2024 Alberto Garlassi


https://apps.dtic.mil/sti/tr/pdf/ADA051001.pdf
https://w7ay.net/site/Technical/RTTY%20Demodulators/Contents/am.html

CW Decoder based on WB7FHC's Morse Code Decoder v1.1
 https://github.com/kareiva/wb7fhc-cw-decoder

 https://www.musicdsp.org/en/latest/Filters/237-one-pole-filter-lp-and-hp.html


This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#define IN_MAIN

#include <time.h>

#include <errno.h>
#include <ncurses.h>

#include "globals.h"

#define DSPINT_DEBUG


#ifdef DSPINT_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#endif

#include "portaudio.h"
#include "pa_ringbuffer.h"
#include "pa_util.h"



#include "fftw3.h"
#include "main.h"



#define FFT_SIZE 2048
#define OVERLAP 128

double x[FFT_SIZE + OVERLAP]; // Input

int i, j, k;


WINDOW *RXWin, *TXWin, *BitErrWin;

uint16_t CharCounter;
float TXLevel;
static long TXSamplesLeft;
float ToneFrequency;
uint8_t TXBit;

uint8_t TXString[255];


float *in;
float *in2;
int nc;
fftwf_complex *out;
fftwf_plan plan_backward;
fftwf_plan plan_forward;

static ring_buffer_size_t rbs_min(ring_buffer_size_t a, ring_buffer_size_t b)
{
    return (a < b) ? a : b;
}


static unsigned NextPowerOf2(unsigned val)
{
    val--;
    val = (val >> 1) | val;
    val = (val >> 2) | val;
    val = (val >> 4) | val;
    val = (val >> 8) | val;
    val = (val >> 16) | val;
    return ++val;
}


/*thread function definition*/
void* RXThread(void* ptr)
{
    paTestData* pData = (paTestData*)ptr;
    static float SumSampleValue;
    while(1)
    {
        ring_buffer_size_t elementsInBuffer = PaUtil_GetRingBufferReadAvailable(&pData->RXringBuffer);
        if ( (elementsInBuffer >= pData->RXringBuffer.bufferSize / NUM_WRITES_PER_BUFFER))
        {
            void* ptr[2] = {0};
            ring_buffer_size_t sizes[2] = {0};

            /* By using PaUtil_GetRingBufferReadRegions, we can read directly from the ring buffer */
            ring_buffer_size_t elementsRead = PaUtil_GetRingBufferReadRegions(&pData->RXringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);
            if (elementsRead > 0)
            {
                int i, j, k;
                for (i = 0; i < 2 && ptr[i] != NULL; ++i)
                {
                    for (j = 0; j < sizes[i]; j+=2)  //only left
                    {
                        SumSampleValue +=  ( * (float *)((uint8_t *)ptr[i] +  pData->RXringBuffer.elementSizeBytes * j));

                        if (k++ == N_OVERSAMPLING - 1)
                        {
                            //                   DecodeSample(SumSampleValue / N_OVERSAMPLING);
                            //			printf("%f\n", SumSampleValue);
                            k = 0;
                            SumSampleValue = 0;
                        }
                    }
                }
                //	                    fwrite(ptr[i], pData->ringBuffer.elementSizeBytes, sizes[i], pData->file);
            }
            PaUtil_AdvanceRingBufferReadIndex(&pData->RXringBuffer, elementsRead);
        }
        Pa_Sleep(10);
    }
}


/*thread function definition*/
void* TXThread(void* ptr)
{
    volatile static double AudioPhase;
    paTestData* pData = (paTestData*)ptr;
    ring_buffer_size_t elementsInBuffer;
    static uint32_t i, j, k, TXBitN;

    uint8_t s[255];
    //	OutData_t TXMessage;
    printf("enter string: ");
    //    My_gets(s);

    //	strncpy((char*) s, "qz3456789abcdefghijklmnopqr\0", 29);
    //strncpy((char*) s, "1", 1);
    //	PrepareBits(s, & TXMessage);

    /*	TXMessage.OutLength = 4;
    TXMessage.OutCodes[0]=0x1;
    TXMessage.OutCodes[1]=0x1;
    TXMessage.OutCodes[2]=0x1;
    TXMessage.OutCodes[3]=0x1;
     */

    i = j = k = TXBitN = 0;

    Pa_Sleep(2000);

    while(1)
    {

        elementsInBuffer = PaUtil_GetRingBufferWriteAvailable(&pData->TXringBuffer);
        if (elementsInBuffer >= pData->TXringBuffer.bufferSize / NUM_WRITES_PER_BUFFER)
        {

            void* ptr[2] = {0};
            ring_buffer_size_t sizes[2] = {0};

            /* By using PaUtil_GetRingBufferWriteRegions, we can write directly into the ring buffer */
            PaUtil_GetRingBufferWriteRegions(&pData->TXringBuffer, elementsInBuffer, ptr + 0, sizes + 0, ptr + 1, sizes + 1);


            ring_buffer_size_t itemsReadFromFile = 0;
            //			int i, j;
            for (i = 0; i < 2 && ptr[i] != NULL; ++i)
            {
                for (j=0; j < (sizes[i] ); j+=2) //only left
                {


                    AudioPhase = 2.f * M_PI * TXSamplesLeft * ToneFrequency / SAMPLE_RATE;
                    *(float *) ((uint8_t *)ptr[i] + pData->TXringBuffer.elementSizeBytes * j) = TXLevel * sin(AudioPhase);
                    *(float *) ((uint8_t *)ptr[i] + pData->TXringBuffer.elementSizeBytes * (j + 1)) = 0;
                    if(TXSamplesLeft > 0)
                        TXSamplesLeft--;

                }
                itemsReadFromFile += sizes[i];
            }
            PaUtil_AdvanceRingBufferWriteIndex(&pData->TXringBuffer, itemsReadFromFile);
        }
        Pa_Sleep(10);
    }
}


/* This routine will be called by the PortAudio engine when audio is needed.
 ** It may be called at interrupt level on some machines so don't do anything
 ** that could mess up the system like calling malloc() or free().
 */
static int AudioCallback( void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData )
{
    paTestData *data = (paTestData*)userData;
    ring_buffer_size_t elementsWriteable = PaUtil_GetRingBufferWriteAvailable(&data->RXringBuffer);
    ring_buffer_size_t elementsToWrite = rbs_min(elementsWriteable, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));
    SAMPLE *rptr = (SAMPLE*)inputBuffer;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    data->RXframeIndex += PaUtil_WriteRingBuffer(&data->RXringBuffer, rptr, elementsToWrite);
    ring_buffer_size_t elementsToPlay = PaUtil_GetRingBufferReadAvailable(&data->TXringBuffer);
    ring_buffer_size_t elementsToRead = rbs_min(elementsToPlay, (ring_buffer_size_t)(framesPerBuffer * NUM_CHANNELS));
    SAMPLE* wptr =  (SAMPLE*)outputBuffer;

    data->TXframeIndex += PaUtil_ReadRingBuffer(&data->TXringBuffer, wptr, elementsToRead);
    return paContinue;

}



void TestAudioRX(void)
{
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data = {0};
    unsigned            numSamples;
    unsigned            numBytes;

    pthread_t RXid, TXid;
    int ret;

    ret=pthread_create(&RXid,NULL,&RXThread,&data);
    if(ret==0)
    {
        printf("RXThread  created successfully.\n");
    }
    else
    {
        printf("RXThread  not created.\n");
        //		return ; /*return from main*/
    }

    ret=pthread_create(&TXid,NULL,&TXThread,&data);
    if(ret==0)
    {
        printf("TXThread  created successfully.\n");
    }
    else
    {
        printf("TXThread  not created.\n");
        return ; /*return from main*/
    }



    printf("patest_record.c\n");
    fflush(stdout);

    /* We set the ring buffer size to about 500 ms */
    numSamples = NextPowerOf2((unsigned)(SAMPLE_RATE * 1.0 * NUM_CHANNELS));
    numBytes = numSamples * sizeof(SAMPLE);
    data.RXringBufferData = (SAMPLE *) malloc( numBytes );
    if( data.RXringBufferData == NULL )
    {
        printf("Could not allocate ring buffer data.\n");
        goto done;
    }

    data.TXringBufferData = (SAMPLE *) malloc( numBytes );
    if( data.TXringBufferData == NULL )
    {
        printf("Could not allocate ring buffer data.\n");
        goto done;
    }

    if (PaUtil_InitializeRingBuffer(&data.RXringBuffer, sizeof(SAMPLE), numSamples, data.RXringBufferData) < 0)
    {
        printf("Failed to initialize ring buffer. Size is not power of 2 ??\n");
        goto done;
    }
    if (PaUtil_InitializeRingBuffer(&data.TXringBuffer, sizeof(SAMPLE), numSamples, data.TXringBufferData) < 0)
    {
        printf("Failed to initialize ring buffer. Size is not power of 2 ??\n");
        goto done;
    }

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice)
    {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice)
    {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    outputParameters.channelCount = 2;                    /* stereo output */
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    nc = ( FRAMES_PER_BUFFER / 2 ) + 1;

    out = fftwf_malloc ( sizeof ( fftwf_complex ) * nc );

    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              &outputParameters,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              AudioCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\n=== Now receiving ===\n");
    fflush(stdout);

    initscr();			/* Start curses mode 		*/
    raw();			/* Line buffering disabled, Pass on everty thing to me 		*/
    noecho();
    RXWin = newwin(6,80,0,0);     //(height, width, starty, startx);
    BitErrWin = newwin(6,80,7,0);     //(height, width, starty, startx);
    TXWin = newwin(5,80,15,0);     //(height, width, starty, startx);
    wtimeout(TXWin, 100);
    scrollok(RXWin,TRUE);
    scrollok(TXWin,TRUE);
    scrollok(BitErrWin,TRUE);
    mvprintw(6,0,"_______________________________________________________________________________________________________");
    mvprintw(14,0,"_______________________________________________________________________________________________________");
    refresh();

    while (1)
    {


        Pa_Sleep(10);
    }
done:
    Pa_Terminate();

    if( err != paNoError )
    {
        fprintf( stderr, "An error occurred while using the portaudio stream \n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
}


void PrintSpectrogram(double *x )
{
    // Create FFTW plans
    fftw_complex *X, *H, *Y;
    fftw_plan fft_x;

    X = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    fft_x = fftw_plan_dft_r2c_1d(FFT_SIZE, x, X, FFTW_ESTIMATE);

    int i, j, ToneReceived;
    float  RXVal, NoiseFloor;
    //for (i = 0; i < L; i += 1)
    for (i = 0; i < 1; i += 1)
    {
        // Copy segment of x into buffer
        double *x_seg = (double *) calloc(FFT_SIZE, sizeof(double));
        for (j = 0; j < FFT_SIZE; j++)
        {
            x_seg[j] = x[i + j];
            //Hanning window
            x_seg[j] *= 0.5 * (1 - cos(2*M_PI*j/(FFT_SIZE - 1)));
//            in[i] *= windowBlackmanHarris(i, transform_size);
        }
        // Perform FFT on segment of x
        fftw_execute_dft_r2c(fft_x, x_seg, X);

        // Print output
#define PRINT_CW
//#define PRINT_SPECTROGRAM
#ifdef PRINT_SPECTROGRAM
        for (j = 50 ; j < 100; j++)
        {
            RXVal = (int)round(10 * log(sqrt(X[j][0] * X[j][0] + X[j][1] * X[j][1])));
            NoiseFloor  = (int)round(10 * log(sqrt(X[j+5][0] * X[j+5][0] + X[j+5][1] * X[j+5][1])));

            NoiseFloor +=  (int)round(10 * log(sqrt(X[j-5][0] * X[j-5][0] + X[j-5][1] * X[j-5][1])));

            NoiseFloor += (int)round(10 * log(sqrt(X[j+6][0] * X[j+6][0] + X[j+6][1] * X[j+6][1])));

            NoiseFloor +=  (int)round(10 * log(sqrt(X[j-6][0] * X[j-6][0] + X[j-6][1] * X[j-6][1])));
            NoiseFloor /= 4;

            //                  printf("%3d", RXVal );
            if (RXVal > -50000)
            {
                printf("%3d", RXVal - NoiseFloor );
            }
            else
            {
                printf("%s", "   " );
            }
        }
        printf("\n");
#endif
#ifdef PRINT_CW

#define FLOOR_OFFSET 10
        ToneReceived = 0;
        float MaxRXVal = 0;
        uint16_t BinMax = 0;
        float FilteredRxVal, FilteredNoiseFloor;
        for (j = 30; j < (100); j++)
        {
            RXVal = (10 * log(sqrt(X[j][0] * X[j][0] + X[j][1] * X[j][1])));
            RXVal += (10 * log(sqrt(X[j+1][0] * X[j+1][0] + X[j+1][1] * X[j+1][1])));
            RXVal /= 2;

            if (RXVal > MaxRXVal)
            {
                MaxRXVal = RXVal;
                NoiseFloor  = (10 * log(sqrt(X[j+FLOOR_OFFSET][0] * X[j+FLOOR_OFFSET][0] + X[j+FLOOR_OFFSET][1] * X[j+FLOOR_OFFSET][1])));
                NoiseFloor +=  (10 * log(sqrt(X[j-FLOOR_OFFSET][0] * X[j-FLOOR_OFFSET][0] + X[j-FLOOR_OFFSET][1] * X[j-FLOOR_OFFSET][1])));
                NoiseFloor += (10 * log(sqrt(X[j+FLOOR_OFFSET + 1][0] * X[j+FLOOR_OFFSET + 1][0] + X[j+FLOOR_OFFSET + 1][1] * X[j+FLOOR_OFFSET + 1][1])));
                NoiseFloor +=  (10 * log(sqrt(X[j-FLOOR_OFFSET - 1][0] * X[j-FLOOR_OFFSET - 1][0] + X[j-FLOOR_OFFSET - 1][1] * X[j-FLOOR_OFFSET - 1][1])));
                NoiseFloor /= 4;
                BinMax = j;
            }
        }
        FilteredRxVal = RxVal_a0 * MaxRXVal - RxVal_b1 * TmpMaxRxVal;
        TmpMaxRxVal = FilteredRxVal;
        FilteredRxVal = MaxRXVal;


        FilteredNoiseFloor = NoiseFloor_a0 * NoiseFloor - NoiseFloor_b1 * TmpNoiseFloor;
        TmpNoiseFloor = FilteredNoiseFloor;
        if (FilteredRxVal - FilteredNoiseFloor > 20)
        {
        //shortens key down
            if (StartSquelchCounter < 10)
            {
                StartSquelchCounter++;
            }
            else
            {
                ToneReceived = 1;
                //          wprintw(RXWin, "%3d", RXVal - NoiseFloor );
                //                     wrefresh(RXWin);
                //    printf("%3d", j);
            }
        }
        else
        {
            StartSquelchCounter = 0;
        }
        //                printf("|%5d %5d %5d", BinMax, (int16_t)FilteredRxVal ,(int16_t) FilteredNoiseFloor );
        if (ToneReceived)
        {
  //          printf("%s", "X" );
            CWIn = 1;
        }
        else
        {
  //          printf("%s", "_");
            CWIn = 0;
        }
        CWSampleCounter++;
        DecodeCW();

        if (CharCounter++ > 128)
        {
            CharCounter = 0;
       //     printf("\n");
        }

#endif
        free(x_seg);
    }

    fftw_destroy_plan(fft_x);
    fftw_free(X);
}



int main(void)
{
    FILE *fp, *fpin;


    fp = fopen( "fileout.raw", "w" );
    errno = 0;
  /*  fpin = fopen("/home/alberto/MonteRosaDecoder/MonteRosaDecoder/bin/Debug/rxlong.raw", "r" );
    if ( errno != 0 )
    {
        perror("Error occurred while opening file.\n");
        exit(1);
    }
  */
    unsigned char buffer[4] = { 0 }; // initialize to 0
    unsigned long c = 0;
    int bytesize = 2; // read in 16 bits
    int SampleCounter = 0;
    int16_t Sample;
    float LowPassParam;

//Coefficient calculation:
//x = exp(-2.0*pi*freq/samplerate);
//a0 = 1.0-x;
//b1 = -x;


    LowPassParam = exp(-2.0*M_PI* 20 /(44100 / 128));
    RxVal_a0 = 1.0-LowPassParam;
    RxVal_b1 = -LowPassParam;

    LowPassParam = exp(-2.0*M_PI* 1 /(44100 / 128));
    NoiseFloor_a0 = 1.0-LowPassParam;
    NoiseFloor_b1 = -LowPassParam;

#ifdef USE_NCURSES
    initscr();			/* Start curses mode 		*/
    raw();			/* Line buffering disabled, Pass on everty thing to me 		*/
    noecho();
    RXWin = newwin(6,80,0,0);     //(height, width, starty, startx);
    BitErrWin = newwin(6,80,7,0);     //(height, width, starty, startx);
    TXWin = newwin(5,80,15,0);     //(height, width, starty, startx);
    wtimeout(TXWin, 100);
    scrollok(RXWin,TRUE);
    scrollok(TXWin,TRUE);
    scrollok(BitErrWin,TRUE);
    mvprintw(6,0,"_______________________________________________________________________________________________________");
    mvprintw(14,0,"_______________________________________________________________________________________________________");
    refresh();
#endif // USE_NCURSES


    while (fread(buffer, bytesize, bytesize, stdin))
        //    while (fread(buffer, bytesize, bytesize, fpin))
    {
// memcpy(&c, buffer, bytesize); // copy the data to a more usable structure for bit manipulation later
        Sample = buffer[0] + 256 * buffer[1];
        if (Sample > 32767)
            Sample = Sample -  65536;

//        fwrite(buffer, 2, 2, fp );
        x[FFT_SIZE + SampleCounter++] = (double) Sample;

        if (SampleCounter == OVERLAP)
        {
            SampleCounter = 0;
            for (k=0; k < FFT_SIZE; k++)
                x[ k] = x[OVERLAP + k];
            PrintSpectrogram(x);
        }
    }
    fclose(stdin);
    printf("rx %c\n", buffer[0]);

//   TestAudioRX();
    //TestCompleteChain();
    //  test_dsp_sample();
    //test_cwmod_decode();
    // printf("size=%d\n",sizeof(ds)+sizeof(df)+sizeof(ps));
}



