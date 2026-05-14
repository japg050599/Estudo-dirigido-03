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

// ==========================================================
// DEFINES DO SISTEMA
// ==========================================================

// Quantidade de pontos armazenados para visualização no Graph Tool
#define SIGNAL_SIZE        80

// Tamanho da janela da média móvel
#define FILTER_SIZE        16

// Período do PWM por software
#define PWM_PERIOD         100

// Valor de PI utilizado na geração da senoide
#define PI_VALUE           3.14159265f


// ==========================================================
// ENUMERAÇÃO DA FSM
// ==========================================================
// Define os possíveis estados do sistema
typedef enum
{
    // Sistema desligado
    STATE_IDLE = 0,

    // Sinal filtrado positivo
    STATE_POSITIVE,

    // Sinal filtrado negativo
    STATE_NEGATIVE

} SystemState_t;


// ==========================================================
// VARIÁVEIS GLOBAIS
// ==========================================================

// Flag que habilita/desabilita a modulação
// Pode ser alterada pelo CCS em tempo real
bool g_enableModulation = true;

// Variável do sinal bruto gerado pelo ADC simulado
float g_rawSignal = 0.0f;

// Variável do sinal filtrado pela média móvel
float g_filteredSignal = 0.0f;


// ==========================================================
// BUFFER DO FILTRO
// ==========================================================

// Buffer circular utilizado pela média móvel
float g_buffer[FILTER_SIZE];

// Índice atual do buffer circular
unsigned int g_bufferIndex = 0;


// ==========================================================
// FSM
// ==========================================================

// Estado atual do sistema
SystemState_t g_state = STATE_IDLE;


// ==========================================================
// PWM
// ==========================================================

// Contador utilizado pelo PWM por software
unsigned int g_pwmCounter = 0;

// Duty-cycle atual
float g_duty = 0.0f;


// ==========================================================
// SENOIDE E VISUALIZAÇÃO
// ==========================================================

// Ângulo atual da senoide
float g_theta = 0.0f;

// Buffer utilizado no Graph Tool para sinal bruto
float g_buffersignal[SIGNAL_SIZE];

// Buffer utilizado no Graph Tool para sinal filtrado
float g_bufferfilteredsignal[SIGNAL_SIZE];

// Índice dos buffers de visualização
unsigned int g_buffersignalIndex = 0;


// ==========================================================
// PROTÓTIPOS DAS FUNÇÕES
// ==========================================================

// Gera sinal ADC simulado
float generateSimulatedADC(void);

// Executa filtro média móvel
float movingAverage(float sample);

// Atualiza a máquina de estados
void updateFSM(void);

// Atualiza o PWM por software
void updatePWM(void);


// ==========================================================
// FUNÇÃO MAIN
// ==========================================================

void main(void)
{
    // Inicializa clock e periféricos básicos do dispositivo
    Device_init();

    // Inicializa módulo de interrupções PIE
    Interrupt_initModule();

    // Inicializa tabela de vetores de interrupção
    Interrupt_initVectorTable();

    // Inicializa GPIOs e Timer configurados pelo SysConfig
	Board_init();

    // Habilita interrupções globais
    EINT;

    // Habilita depuração em tempo real
    ERTM;

    // Loop infinito principal
    // Todo processamento ocorre na ISR do timer
    while(1)
    {
    }	
	
}


// ==========================================================
// ISR DO TIMER
// ==========================================================
// Executada periodicamente pelo CPU Timer configurado no SysConfig

__interrupt void INT_Led_Toggle_Timer_ISR(void)
{

 // =====================================================
    // 1. ADC SIMULADO
    // =====================================================

    // Gera senoide com ruído
    g_rawSignal = generateSimulatedADC();

    // Armazena sinal bruto para visualização
    // para visualizar no grah tool os sinais centralizados em zero, colocar um -2048.0f
    g_buffersignal[g_buffersignalIndex] =
            g_rawSignal ;


    // =====================================================
    // 2. FILTRO
    // =====================================================

    // Executa média móvel
    g_filteredSignal = movingAverage(g_rawSignal);

    // Armazena sinal filtrado para visualização
    g_bufferfilteredsignal[g_buffersignalIndex] =
            g_filteredSignal ;

    // Incrementa índice circular do buffer
    g_buffersignalIndex =
            (g_buffersignalIndex + 1) % SIGNAL_SIZE;

    // Remove offset DC para análise da FSM , para poder ver os semiciclos positivos e negativos da senoide através dos LEDS
    g_filteredSignal = g_filteredSignal - 2048.0f ;


    // =====================================================
    // 3. FSM
    // =====================================================

    // Atualiza máquina de estados
    updateFSM();


    // =====================================================
    // 4. PWM SOFTWARE
    // =====================================================

    // Atualiza LEDs com PWM por software
    updatePWM();


    // Limpa flag da interrupção do PIE
    Interrupt_clearACKGroup(
            INT_Led_Toggle_Timer_INTERRUPT_ACK_GROUP);
}


// ==========================================================
// ADC SIMULADO
// ==========================================================
// Gera uma senoide com ruído aleatório

float generateSimulatedADC(void)
{
    float noise;

    // Senoide centrada em 2048 - Obs: para ver os leds variando tirar o valor médio 2048.0f
    // Amplitude de 1000
    float signal = 2048.0f + 1000.0f * sinf(g_theta);

    // Gera ruído aleatório entre -50 e +50
    noise =   ((float)(rand() % 100) - 50.0f);

    // Incrementa ângulo da senoide
    // 80 pontos correspondem a um período completo
    g_theta += 0.07854f;

    // Mantém theta dentro do intervalo 0 até 2PI
    // Evita descontinuidade na senoide
    if(g_theta >= 2.0f * PI_VALUE)
    {
        g_theta -= 2.0f * PI_VALUE;
    }

    // Retorna senoide + ruído
    return signal + noise;
}


// ==========================================================
// FILTRO MÉDIA MÓVEL
// ==========================================================
// Implementação utilizando buffer circular

float movingAverage(float sample)
{
    float sum = 0.0f;
    unsigned int i;

    // Insere nova amostra no buffer
    g_buffer[g_bufferIndex] = sample;

    // Incrementa índice do buffer
    g_bufferIndex++;

    // Reinicia índice ao atingir final do buffer
    if(g_bufferIndex >= FILTER_SIZE)
    {
        g_bufferIndex = 0;
    }

    // Soma todas as amostras do buffer
    for(i = 0; i < FILTER_SIZE; i++)
    {
        sum += g_buffer[i];
    }

    // Retorna média das amostras
    return sum / FILTER_SIZE;
}


// ==========================================================
// FSM
// ==========================================================
// Define estado atual baseado no sinal filtrado

void updateFSM(void)
{
    // Se modulação desabilitada
    // entra no estado IDLE
    if(g_enableModulation == false)
    {
        g_state = STATE_IDLE;
        return;
    }

    // Sinal positivo
    if(g_filteredSignal > 0)
    {
        g_state = STATE_POSITIVE;
    }

    // Sinal negativo
    else
    {
        g_state = STATE_NEGATIVE;
    }
}


// ==========================================================
// PWM POR SOFTWARE
// ==========================================================
// Controla brilho dos LEDs

void updatePWM(void)
{
    float absValue;

    // Incrementa contador do PWM
    g_pwmCounter++;

    // Reinicia contador ao atingir período
    if(g_pwmCounter >= PWM_PERIOD)
    {
        g_pwmCounter = 0;
    }

    // LEDs inicialmente desligados
    // LEDs da LaunchPad são ativos em nível baixo
    GPIO_writePin(myBoardLED0_GPIO, 1);
    GPIO_writePin(myBoardLED1_GPIO, 1);

    // Estado IDLE mantém LEDs desligados
    if(g_state == STATE_IDLE)
    {
        return;
    }

    // Obtém valor absoluto do sinal
    absValue = fabsf(g_filteredSignal);

    // Duty-cycle proporcional à amplitude do sinal
    g_duty = absValue / 1000.0f;

    // Limita duty máximo em 100%
    if(g_duty > 1.0f)
    {
        g_duty = 1.0f;
    }

    // Calcula valor de comparação do PWM
    unsigned int compare =
            (unsigned int)(g_duty * PWM_PERIOD);

    // Estado positivo controla LED verde
    if(g_state == STATE_POSITIVE)
    {
        if(g_pwmCounter < compare)
        {
            GPIO_writePin(myBoardLED0_GPIO, 0);
        }
    }

    // Estado negativo controla LED azul
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