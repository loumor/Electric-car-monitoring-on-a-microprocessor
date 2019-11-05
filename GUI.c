/*
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== GUI.c ========
 */

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Types.h>
#include <xdc/runtime/Error.h>


/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/hal/Seconds.h>


/* TI-RTOS Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART.h>

/* Example/Board Header files */
#include "Board.h"

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "grlib/grlib.h"
#include "grlib/widget.h"
#include "grlib/canvas.h"
#include "grlib/pushbutton.h"

#include "drivers/Kentec320x240x16_ssd2119_spi.h"
#include "drivers/touch.h"
#include "drivers/images.h"
#include "drivers/pinout.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/rom_map.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"


#define TASKSTACKSIZE     2048

/*
 * ====== Structs =======
 */
// Hold the user selected screen
typedef struct GUIparam{
    uint8_t screen;
    // We are able to place information in this Struct relating to the current screen
}GUIparam;

/*
 * ===== Variables =====
 */
tContext sContext;
GUIparam guiParam; // GUI container
Task_Struct task0Struct; // Main Task
Char task0Stack[TASKSTACKSIZE];
Types_FreqHz cpufreq;


//Global Handles to Hwi & Swi threads
Hwi_Handle myHwi;
Swi_Handle mySwi;
Task_Handle GUITask;
Error_Block eb;

/*
 * ======= GUI Handlers =====
 */

extern tCanvasWidget g_sHomePage;
extern tPushButtonWidget g_sNextPage;
extern tPushButtonWidget g_sIncrease;
extern tPushButtonWidget g_sDecrease;
extern tPushButtonWidget g_sWeatherClassify;
extern tPushButtonWidget g_sMotorSpeed;
extern tPushButtonWidget g_sUpperCurrent;
extern tPushButtonWidget g_sUpperTemp;
extern tPushButtonWidget g_sUpperAccel;
extern tPushButtonWidget g_sFaultDis;


void OnNext();

// Home Page
Canvas(g_sHomePage, WIDGET_ROOT, 0, 0,
       &g_sKentec320x240x16_SSD2119, 10, 25, 300, (240 - 25 -10),
       CANVAS_STYLE_FILL, ClrBlack, 0, 0, 0, 0, 0, 0);

// Next Page Button
RectangularButton(g_sNextPage, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 270, 190, 50, 50,
                  (PB_STYLE_OUTLINE | PB_STYLE_TEXT_OPAQUE | PB_STYLE_TEXT |
                    PB_STYLE_FILL | PB_STYLE_RELEASE_NOTIFY),
                   ClrBlack, ClrWhite, ClrWhite, ClrWhite,
                   g_psFontCmss18b, "Next", 0, 0, 0, 0, OnNext);

// Increase Button
RectangularButton(g_sIncrease, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 210, 150,
                  50, 50, PB_STYLE_IMG | PB_STYLE_TEXT, ClrBlack, ClrBlack, 0,
                  ClrSilver, &g_sFontCm20, "+", g_pui8Blue50x50,
                  g_pui8Blue50x50Press, 0, 0, NULL);

// Decrease Button
RectangularButton(g_sDecrease, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 60, 150,
                  50, 50, PB_STYLE_IMG | PB_STYLE_TEXT, ClrBlack, ClrBlack, 0,
                  ClrSilver, &g_sFontCm20, "-", g_pui8Blue50x50,
                  g_pui8Blue50x50Press, 0, 0, NULL);

// Weather Classify Text
RectangularButton(g_sWeatherClassify, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 85, 40, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "WEATHER", 0, 0, 0, 0, NULL);

// Motor Speed Setting
RectangularButton(g_sMotorSpeed, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 5, 80, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "MOTOR", 0, 0, 0, 0, NULL);

// Current Setting
RectangularButton(g_sUpperCurrent, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 5, 115, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "CURRENT", 0, 0, 0, 0, NULL);

// Temperature Setting
RectangularButton(g_sUpperTemp, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 165, 80, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "TEMP", 0, 0, 0, 0, NULL);

// Acceleration Setting
RectangularButton(g_sUpperAccel, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 165, 115, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "ACCEL", 0, 0, 0, 0, NULL);

// Fault Display Text
RectangularButton(g_sFaultDis, &g_sHomePage, 0, 0,
                  &g_sKentec320x240x16_SSD2119, 85, 210, 150, 25,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_TEXT,
                  ClrWhite, ClrWhite, ClrRed, ClrBlack,
                  &g_sFontCm22, "Fault:..", 0, 0, 0, 0, NULL);


/*
 * ====== Functions ======
 *
 */


// Clicking Next Button Handler
void OnNext()
{

    guiParam.screen += 1;

}

// The Home Page screen
static void draw_HomePage(){

    tRectangle sRect;

    // DEBUGGING
    System_printf("TESTING: Entered Draw Home page function\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Set up DATE/TIME banner */
    // Fill the top 24 rows of the screen with blue to create the banner.
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
    sRect.i16YMax = 23;
    GrContextForegroundSet(&sContext, ClrDarkBlue);
    GrRectFill(&sContext, &sRect);
    // Put a white box around the banner.
    GrContextForegroundSet(&sContext, ClrWhite);
    GrRectDraw(&sContext, &sRect);

    // Put the application name in the middle of the banner.
    GrContextFontSet(&sContext, &g_sFontCm20);
    GrStringDrawCentered(&sContext, "PUT DATE AND TIME IN HERE", -1,
                             GrContextDpyWidthGet(&sContext) / 2, 8, 0);



    WidgetPaint(WIDGET_ROOT);

    while (1){

        if (guiParam.screen != 0){
            break;
        }

        WidgetMessageQueueProcess();

    }
}


// The Second Page screen
static void draw_SecondPage(){

    // DEBUGGING
    System_printf("TESTING: Entered Draw Second page function\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    while (1){


        WidgetMessageQueueProcess();

    }
}


/*
 *  ======== User Interface Build ========
 */
Void userInterFxn(UArg arg0, UArg arg1)
{
    // arg0 is the System Clock Time

    /* Initiate the interface */

    PinoutSet(false, false);

    // Initialize the display driver.
    Kentec320x240x16_SSD2119Init(arg0);

    // Initialize the graphics context.
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);

    // Initialize the touch screen driver and have it route its messages to the
    // widget tree.
    TouchScreenInit(arg0);
    TouchScreenCallbackSet(WidgetPointerMessage);

    // DEBUGGING/TESTING
    System_printf("TESTING: Passed Display/Graphics construction\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    // Select the Home Screen for initial startup
    guiParam.screen = 0;

    /* Add the widgets */
    // Home Page
    WidgetAdd(WIDGET_ROOT, (tWidget *)&g_sHomePage);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sNextPage);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sIncrease);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sDecrease);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sWeatherClassify);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sMotorSpeed);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sUpperCurrent);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sUpperTemp);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sUpperAccel);
    WidgetAdd((tWidget *)&g_sHomePage, (tWidget *)&g_sFaultDis);


    /* Run the GUI */
    while (1){

        // DEBUGGING
        System_printf("TESTING: Made into while loop of GUI Drawing\n");
        /* SysMin will only print to the console when you call flush or exit */
        System_flush();

        // Draw the Interface relating to user selection
        switch(guiParam.screen)
            {
                case 0:
                    draw_HomePage();
                    break;
                case 1:
                    draw_SecondPage();
                    break;
            }
    }
}

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initI2C();
    Board_initUART();

    /* Set up the clock */
    // Set the clocking to run directly from the crystal at 120MHz.
    uint32_t ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                                    SYSCTL_OSC_MAIN | SYSCTL_USE_PLL |
                                                    SYSCTL_CFG_VCO_480), 120000000);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    TimerConfigure(TIMER1_BASE, TIMER_CFG_PERIODIC);
    TimerPrescaleSet(TIMER1_BASE, TIMER_A , 16 );
    TimerLoadSet(TIMER1_BASE, TIMER_A, ui32SysClock);
    TimerEnable(TIMER1_BASE, TIMER_A);

    // DEBUGGING/TESTING
    System_printf("TESTING: Passed Clock construction on Timer 1\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();


    /* Construct BIOS objects */
    Task_Params taskParams;
    Task_Params_init(&taskParams);
    taskParams.stackSize = TASKSTACKSIZE;
    taskParams.stack = &task0Stack;
    taskParams.priority = 5;
    taskParams.arg0 = ui32SysClock; // Pass the system clock to the GUI
    taskParams.instance->name = "interface";
    GUITask = Task_create((Task_FuncPtr)userInterFxn, &taskParams, NULL);

    // DEBUGGING/TESTING
    GPIO_write(Board_LED0, Board_LED_ON);
    System_printf("TESTING: Passed Task Construction\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
