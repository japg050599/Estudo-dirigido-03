//############################################################################
//
// FILE:lab_main.c
//
// TITLE: Lab - Blink two LEDs using SysConfig and CPU timer
//
// C2K ACADEMY URL: https://dev.ti.com/tirex/local?id=source_c2000_get_started_c2000_getstarted_sysconfig&packageId=C2000-ACADEMY
//
//! \addtogroup academy_lab_list
//! <h1> Using SysConfig Lab </h1>
//!
//! TI code composer studio includes a GUI-based for system configuration tool
//! (SysConfig). It can be used for pinmux configuration, peripheral setup, and
//! generation of c-code using C2000Ware DriverLib API. The generated c-code is
//! automatically included in the project. The lab exercise in this module is
//! broken into two parts: * Create blinky_led example using sysconfig GUI
//! tool. This will be similar to lab excercise in module 1 - blinks two LEDs
//! on the board. * Add CPU-Timer configuration, and timer interrupt-handler to
//! control the frequency of LEDs toggle
//!
//! \b External \b Connections \n
//!  - None.
//!
//! \b Watch \b Variables \n
//!  - None.
//!
//############################################################################
// $Copyright:
// Copyright (C) 2022 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//############################################################################

#include "driverlib.h"
#include "device.h"
#include "board.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#define SIGNAL_SIZE        80
#define FILTER_SIZE        16
#define PWM_PERIOD         100
#define PI_VALUE           3.14159265f

typedef enum
{
    STATE_IDLE = 0,
    STATE_POSITIVE,
    STATE_NEGATIVE

} SystemState_t;

// ===================== VARIÁVEIS GLOBAIS =====================

// Flag habilitação
bool g_enableModulation = true;

// ADC bruto e filtrado
float g_rawSignal = 0.0f;
float g_filteredSignal = 0.0f;

// Buffer média móvel
float g_buffer[FILTER_SIZE];
unsigned int g_bufferIndex = 0;

// FSM
SystemState_t g_state = STATE_IDLE;

// PWM
unsigned int g_pwmCounter = 0;
float g_duty = 0.0f;

// senoide
float g_theta = 0.0f;
float g_buffersignal[SIGNAL_SIZE];
float g_bufferfilteredsignal[SIGNAL_SIZE];
unsigned int g_buffersignalIndex = 0;

// ===================== PROTÓTIPOS =====================

float generateSimulatedADC(void);
float movingAverage(float sample);
void updateFSM(void);
void updatePWM(void);


// ===================== MAIN ===========================

void main(void)
{
    // Device Initialization
    Device_init();

    //
    // Initializes PIE and clears PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initializes the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    //
    Interrupt_initVectorTable();

	Board_init();

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;

    while(1)
    {
    }	
	
}

// ===================== ISR TIMER =====================

__interrupt void INT_Led_Toggle_Timer_ISR(void)
{
 //   GPIO_togglePin(myBoardLED0_GPIO);
 //   GPIO_togglePin(myBoardLED1_GPIO);

 // =====================================================
    // 1. ADC SIMULADO
    // =====================================================

    g_rawSignal = generateSimulatedADC();
    g_buffersignal[g_buffersignalIndex] = g_rawSignal;
    

    // =====================================================
    // 2. FILTRO
    // =====================================================

    g_filteredSignal = movingAverage(g_rawSignal);
    g_bufferfilteredsignal[g_buffersignalIndex] = g_filteredSignal;

    g_buffersignalIndex = (g_buffersignalIndex + 1)%SIGNAL_SIZE;

    // Centraliza em zero
    g_filteredSignal = g_filteredSignal - 2048.0f;

    // =====================================================
    // 3. FSM
    // =====================================================

    updateFSM();

    // =====================================================
    // 4. PWM SOFTWARE
    // =====================================================

    updatePWM();


    Interrupt_clearACKGroup(INT_Led_Toggle_Timer_INTERRUPT_ACK_GROUP);
}

// ===================== ADC SIMULADO =====================

float generateSimulatedADC(void)
{
    float noise;

    // senoide centrada em 2048
    float signal = 2048.0f + 1000.0f * sinf(g_theta);

    // ruído +-50
    noise = ((float)(rand() % 100) - 50.0f);

    //g_theta += 0.08f;
    g_theta += 0.07854f;

    if(g_theta >= 2.0f * PI_VALUE)
    {
        g_theta = 0.0f;
        //g_theta -= 2.0f * PI_VALUE;
    }

    return signal + noise;
}

// ===================== MÉDIA MÓVEL =====================

float movingAverage(float sample)
{
    float sum = 0.0f;
    unsigned int i;

    g_buffer[g_bufferIndex] = sample;

    g_bufferIndex++;

    if(g_bufferIndex >= FILTER_SIZE)
    {
        g_bufferIndex = 0;
    }

    for(i = 0; i < FILTER_SIZE; i++)
    {
        sum += g_buffer[i];
    }

    return sum / FILTER_SIZE;
}


// ===================== FSM =====================

void updateFSM(void)
{
    if(g_enableModulation == false)
    {
        g_state = STATE_IDLE;
        return;
    }

    if(g_filteredSignal > 0)
    {
        g_state = STATE_POSITIVE;
    }
    else
    {
        g_state = STATE_NEGATIVE;
    }
}


// ===================== PWM =====================

void updatePWM(void)
{
    float absValue;

    g_pwmCounter++;

    if(g_pwmCounter >= PWM_PERIOD)
    {
        g_pwmCounter = 0;
    }

    // LEDs OFF inicialmente
    GPIO_writePin(myBoardLED0_GPIO, 1);
    GPIO_writePin(myBoardLED1_GPIO, 1);

    if(g_state == STATE_IDLE)
    {
        return;
    }

    absValue = fabsf(g_filteredSignal);

    // duty proporcional
    g_duty = absValue / 1000.0f;

    if(g_duty > 1.0f)
    {
        g_duty = 1.0f;
    }

    unsigned int compare = (unsigned int)(g_duty * PWM_PERIOD);

    if(g_state == STATE_POSITIVE)
    {
        if(g_pwmCounter < compare)
        {
            GPIO_writePin(myBoardLED0_GPIO, 0);
        }
    }

    if(g_state == STATE_NEGATIVE)
    {
        if(g_pwmCounter < compare)
        {
            GPIO_writePin(myBoardLED1_GPIO, 0);
        }
    }
}

//
// End of File
//
