/* CW Decoder based on WB7FHC's Morse Code Decoder v1.1
 * https://github.com/kareiva/wb7fhc-cw-decoder
*/
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "pa_ringbuffer.h"
#include "globals.h"
#include "main.h"
#include "logger.h"

#define true (1)
#define false (0)

int audio = 1;            // will store the value we read on this pin

int LCDline = 1;          // keeps track of which line we're printing on
int lineEnd = 21;         // One more than number of characters across display
int letterCount = 0;      // keeps track of how may characters were printed on the line
int lastWordCount = 0;    // keeps track of how may characters are in the current word
int lastSpace = 0;        // keeps track of the location of the last 'space'



// The next line stores the text that we are currently printing on a line,
// The characters in the current word,
// Our top line of text,
// Our second line of text,
// and our third line of text
// For a 20x4 display these are all 20 characters long
char currentLine[] = "12345678901234567890";
char    lastWord[] = "                    ";
char       line1[] = "                    ";
char       line2[] = "                    ";
char       line3[] = "                    ";

uint8_t ditOrDah = true;  // We have either a full dit or a full dah
int dit = 35;             // We start by defining a dit as 10 milliseconds

// The following values will auto adjust to the sender's speed
static int averageDah = 80;             // A dah should be 3 times as long as a dit
int averageWordGap = 80;  // will auto adjust
long fullWait = 6000;             // The time between letters
long waitWait = 6000;             // The time between dits and dahs
long newWord = 0;                 // The time between words

uint8_t characterDone = true; // A full character has been sent

int downTime = 0;        // How long the tone was on in milliseconds
int upTime = 0;          // How long the tone was off in milliseconds
int myBounce = 2;        // Used as a short delay between key up and down

static long startDownTime = 0;  // Arduino's internal timer when tone first comes on
static long startUpTime = 0;    // Arduino's internal timer when tone first goes off

static long lastDahTime = 0;    // Length of last dah in milliseconds
static long lastDitTime = 0;    // Length oflast dit in milliseconds
static long averageDahTime = 0; // Sloppy Average of length of dahs

uint8_t justDid = true; // Makes sure we only print one space during long gaps

int myNum = 0;           // We will turn dits and dahs into a binary number stored here

/////////////////////////////////////////////////////////////////////////////////
// Now here is the 'Secret Sauce'
// The Morse Code is embedded into the binary version of the numbers from 2 - 63
// The place a letter appears here matches myNum that we parsed out of the code
// #'s are miscopied characters
char mySet[] ="##TEMNAIOGKDWRUS##QZYCXBJP#L#FVH09#8###7#####/-61#######2###3#45";
char lcdGuy = ' ';       // We will store the actual character decoded here
char JS8String[256];
/////////////////////////////////////////////////////////////////////////////////

void ExtractPayload(char InChar)
{
    if (OutStringCounter > 1)
    {
        if (InChar == 'W')
        {
            //              OutString[OutStringCounter++] = 'z';
            OutString[OutStringCounter] = '\0';
            time_t now = time(&now);
            struct tm tm = *gmtime(&now);
            sprintf(JS8String,"IQ2MI/B: %d-%02d-%02d %02d:%02d:%02d %s\n\0", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, OutString);
            OutStringCounter = 0;

            FILE *JS8StringF = fopen( "/home/alberto/develop-GW-JW8/GW-JS8/IQ2MI_telemetry", "w" );
            fputs(JS8String, JS8StringF);
            fclose(JS8StringF);
            printf("\n\n%s\n\n", JS8String);
            LOG_INFO("Decoded: %s", JS8String);
            logger_flush();
        }
        else
        {
            OutString[OutStringCounter++] = InChar;
            if (OutStringCounter >= MAX_OUTSTRINGCOUNTER)
                OutStringCounter = 0;
        }
    }


    if ( OutStringCounter == 1)
    {
        if (InChar == 'A')
        {
            OutString[OutStringCounter++] = InChar;
            if (OutStringCounter >= MAX_OUTSTRINGCOUNTER)
                OutStringCounter = 0;
        }
        else
            OutStringCounter = 0;
    }


    if ( OutStringCounter == 0)
    {
        if (InChar == 'B')
        {
            OutString[OutStringCounter++] = InChar;
            if (OutStringCounter >= MAX_OUTSTRINGCOUNTER)
                OutStringCounter = 0;
        }
        else
            OutStringCounter = 0;
    }
}

void DecodeCW(void)
{
    // 12 WPM is 5 Hz
    //We are limited by the FFT execution rate


    if (CWIn > 0) keyIsDown();       // tone is being decoded
    else keyIsUp();          //  no tone is there
}

void keyIsDown(void)
{
    // The decoder is detecting our tone




    if (startUpTime>0)
    {
        // We only need to do once, when the key first goes down
        startUpTime=0;    // clear the 'Key Up' timer
    }
    // If we haven't already started our timer, do it now
    if (startDownTime == 0)
    {
        startDownTime = CWSampleCounter;  // get Arduino's current clock time
    }

    characterDone=false; // we're still building a character
    ditOrDah=false;      // the key is still down we're not done with the tone
    //TODO check if it is really needed     delay(myBounce);     // Take a short breath here

    if (myNum == 0)        // myNum will equal zero at the beginning of a character
    {
        myNum = 1;          // This is our start bit  - it only does this once per letter
    }
}

void keyIsUp()
{
    // The decoder does not detect our tone


    // If we haven't already started our timer, do it now
    if (startUpTime == 0)
    {
        startUpTime = CWSampleCounter;
    }

    // Find out how long we've gone with no tone
    // If it is twice as long as a dah print a space
    upTime = CWSampleCounter - startUpTime;
    if (upTime<10)return;
    if (upTime > (averageDah*150/100))
    {
        printSpace();
    }

    // Only do this once after the key goes up
    if (startDownTime > 0)
    {
        downTime = CWSampleCounter - startDownTime;  // how long was the tone on?
        startDownTime=0;      // clear the 'Key Down' timer
    }

    if (!ditOrDah)
    {
        // We don't know if it was a dit or a dah yet
        shiftBits();    // let's go find out! And do our Magic with the bits
    }

    // If we are still building a character ...
    if (!characterDone)
    {
        // Are we done yet?
        if (upTime > dit)
        {
            // BINGO! we're done with this one
            printCharacter();       // Go figure out what character it was and print it
            characterDone=true;     // We got him, we're done here
            myNum=0;                // This sets us up for getting the next start bit
        }
        downTime=0;               // Reset our keyDown counter
    }
}


void shiftBits()
{
    // we know we've got a dit or a dah, let's find out which
    // then we will shift the bits in myNum and then add 1 or not add 1

    //	if (downTime < dit / 3) return;  // ignore my keybounce //TODO serve? blocca l'autoregolazione
    if (downTime < 10) return;

    myNum = myNum << 1;   // shift bits left
    ditOrDah = true;        // we will know which one in two lines


    // If it is a dit we add 1. If it is a dah we do nothing!
    if (downTime < dit)
    {
        myNum++;           // add one because it is a dit
    }
    LastPulsesRatio =  (float)downTime/ LastDownTime;
    if (((LastPulsesRatio > 1.8) && (LastPulsesRatio < 5.0)) || ((LastPulsesRatio > (1/1.8)) && (LastPulsesRatio < (1 / 5.0))))
    {
        if (LastPulsesRatio > 1)
        {
            averageDah = downTime;
        }
        else
        {
            averageDah = LastDownTime;
        }
        averageDah = (downTime + 7 * averageDah) / 8;  // running average of dahs
        CurrentAverageDah = averageDah;
        if (averageDah > 400)
            averageDah = 400;
        if (averageDah < 50)
            averageDah = 50;

        dit = averageDah / 3;                    // normal dit would be this
        dit = dit * 2;    // double it to get the threshold between dits and dahs
    }
    LastDownTime = downTime;
}


void printCharacter()
{
    justDid = false;         // OK to print a space again after this

    // Punctuation marks will make a BIG myNum
    if (myNum > 63)
    {
        printPunctuation();  // The value we parsed is bigger than our character array
        // It is probably a punctuation mark so go figure it out.
        return;              // Go back to the main loop(), we're done here.
    }
    lcdGuy = mySet[myNum]; // Find the letter in the character set
    sendToLCD();           // Go figure out where to put in on the display
}

void printSpace()
{
    if (justDid) return;  // only one space, no matter how long the gap
    justDid = true;       // so we don't do this twice

    // We keep track of the average gap between words and bump it up 20 milliseconds
    // do avoid false spaces within the word
    averageWordGap = ((averageWordGap + upTime) / 2) + 20;

    lastWordCount=0;      // start counting length of word again
    currentLine[letterCount]=' ';  // and a space to the variable that stores the current line
    lastSpace=letterCount;         // keep track of this, our last, space

    // Now we need to clear all the characters out of our last word array
    for (int i=0; i<20; i++)
    {
        lastWord[i]=' ';
    }

    lcdGuy=' ';         // this is going to go to the LCD

    // We don't need to print the space if we are at the very end of the line
    if (letterCount < 20)
    {
        sendToLCD();         // go figure out where to put it on the display
    }
}

void printPunctuation()
{
    // Punctuation marks are made up of more dits and dahs than
    // letters and numbers. Rather than extend the character array
    // out to reach these higher numbers we will simply check for
    // them here. This funtion only gets called when myNum is greater than 63

    // Thanks to Jack Purdum for the changes in this function
    // The original uses if then statements and only had 3 punctuation
    // marks. Then as I was copying code off of web sites I added
    // characters we don't normally see on the air and the list got
    // a little long. Using 'switch' to handle them is much better.


    switch (myNum)
    {
    case 71:
        lcdGuy = ':';
        break;
    case 76:
        lcdGuy = ',';
        break;
    case 84:
        lcdGuy = '!';
        break;
    case 94:
        lcdGuy = '-';
        break;
    case 97:
        lcdGuy = 39;    // Apostrophe
        break;
    case 101:
        lcdGuy = '@';
        break;
    case 106:
        lcdGuy = '.';
        break;
    case 115:
        lcdGuy = '?';
        break;
    case 246:
        lcdGuy = '$';
        break;
    case 122:
        lcdGuy = 's';
        //		sendToLCD(); //TODO use other character
        lcdGuy = 'k';
        break;
    default:
        lcdGuy = '#';    // Should not get here
        break;
    }
    sendToLCD();    // go figure out where to put it on the display
}

void sendToLCD()
{

//	printf("    %c    ", lcdGuy);
    printf("%c", lcdGuy);
    fflush(stdout);
    ExtractPayload(lcdGuy);
}

