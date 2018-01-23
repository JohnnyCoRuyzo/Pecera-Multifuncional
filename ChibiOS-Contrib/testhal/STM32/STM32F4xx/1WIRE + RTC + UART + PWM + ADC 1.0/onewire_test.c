/*
    ChibiOS/RT - Copyright (C) 2014 Uladzimir Pylinsky aka barthess

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string.h>

#include "hal.h"

/*
 ******************************************************************************
 * DEFINES
 ******************************************************************************
 */

#if defined(BOARD_ST_NUCLEO_F401RE)
  #if ONEWIRE_USE_STRONG_PULLUP
  #error "This board has not enough voltage for this feature"
  #endif
#endif

  #define ONEWIRE_PORT                  GPIOB
  #define ONEWIRE_PIN                   GPIOB_PIN0
  #define ONEWIRE_PAD_MODE_ACTIVE       (PAL_MODE_ALTERNATE(2) | PAL_STM32_OTYPE_OPENDRAIN)
  #define search_led_off()              (palClearPad(GPIOA, GPIOA_PIN4))
  #define search_led_on()               (palSetPad(GPIOA, GPIOA_PIN4))
  #define ONEWIRE_MASTER_CHANNEL        2
  #define ONEWIRE_SAMPLE_CHANNEL        3

/*
 ******************************************************************************
 * EXTERNS
 ******************************************************************************
 */

/*
 ******************************************************************************
 * PROTOTYPES
 ******************************************************************************
 */
/*
 * Forward declarations
 */
#if ONEWIRE_USE_STRONG_PULLUP
static void strong_pullup_assert(void);
static void strong_pullup_release(void);
#endif

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

static uint8_t testbuf[12];

static int32_t temperature;

/*
 * Config for underlied PWM driver.
 * Note! It is NOT constant because 1-wire driver needs to change them
 * during functioning.
 */
static PWMConfig pwm_cfg = {
    0,
    0,
    NULL,
    {
     {PWM_OUTPUT_DISABLED, NULL},
     {PWM_OUTPUT_DISABLED, NULL},
     {PWM_OUTPUT_DISABLED, NULL},
     {PWM_OUTPUT_DISABLED, NULL}
    },
    0,
#if STM32_PWM_USE_ADVANCED
    0,
#endif
    0
};

/*
 *
 */
static const onewireConfig ow_cfg = {
    &PWMD3,
    &pwm_cfg,
    PWM_OUTPUT_ACTIVE_LOW,
    ONEWIRE_MASTER_CHANNEL,
    ONEWIRE_SAMPLE_CHANNEL,
    ONEWIRE_PORT,
    ONEWIRE_PIN,
    ONEWIRE_PAD_MODE_ACTIVE,
#if ONEWIRE_USE_STRONG_PULLUP
    strong_pullup_assert,
    strong_pullup_release
#endif
};

/*
 ******************************************************************************
 ******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************
 ******************************************************************************
 */

#if ONEWIRE_USE_STRONG_PULLUP
/**
 *
 */
static void strong_pullup_assert(void) {
  palSetPadMode(ONEWIRE_PORT, ONEWIRE_PIN, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
}

/**
 *
 */
static void strong_pullup_release(void) {
  palSetPadMode(ONEWIRE_PORT, ONEWIRE_PIN, PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
}
#endif /* ONEWIRE_USE_STRONG_PULLUP */

/*
 ******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************
 */

/**
 *
 */
int32_t onewireTest(void) {

  int16_t tmp;
  uint8_t rombuf[24];
  size_t devices_on_bus = 0;
  bool presence;
  bool x = true;

  onewireObjectInit(&OWD1);
  onewireStart(&OWD1, &ow_cfg);

#if ONEWIRE_SYNTH_SEARCH_TEST
  synthSearchRomTest(&OWD1);
#endif

    temperature = -666;

  while (x) {
    if (true == onewireReset(&OWD1)){

      memset(rombuf, 0x55, sizeof(rombuf));
      search_led_on();
      devices_on_bus = onewireSearchRom(&OWD1, rombuf, 3); //Direccion en la que se verifican 
      //los dispositivos conectados, y numero máximo de dispositivos que se van a conectar 
      search_led_off();
      osalDbgCheck(devices_on_bus <= 3);
      osalDbgCheck(devices_on_bus  > 0);

      if (1 == devices_on_bus){
        /* test read rom command */
        presence = onewireReset(&OWD1);
        osalDbgCheck(true == presence);
        testbuf[0] = ONEWIRE_CMD_READ_ROM;
        onewireWrite(&OWD1, testbuf, 1, 0);
        onewireRead(&OWD1, testbuf, 8);
        osalDbgCheck(testbuf[7] == onewireCRC(testbuf, 7));
        osalDbgCheck(0 == memcmp(rombuf, testbuf, 8));
      }

      /* configure temperature measurement on the devices */
      presence = onewireReset(&OWD1);
      osalDbgCheck(true == presence);
      testbuf[0] = ONEWIRE_CMD_SKIP_ROM;
      onewireWrite(&OWD1, testbuf, 1, 0);
      testbuf[0] = ONEWIRE_CMD_WRITE_SCRATCHPAD;
      onewireWrite(&OWD1, testbuf, 1, 0);
      testbuf[1] = 0x00; //segundo bit de temperatura 0
      testbuf[2] = 0x64; //temperatura maxima
      testbuf[3] = 0x00; //temperatura minima
      testbuf[4] = 0x1F; //configuración de 9 bits
      onewireWrite(&OWD1, testbuf, 5, 0);


      /* start temperature measurement on all connected devices at once */
      presence = onewireReset(&OWD1);
      osalDbgCheck(true == presence);
      testbuf[0] = ONEWIRE_CMD_SKIP_ROM;
      testbuf[1] = ONEWIRE_CMD_CONVERT_TEMP;
      onewireWrite(&OWD1, testbuf, 2, 0);

      ow_bus_idle(&OWD1);
      chThdSleepMilliseconds(100);

#if ONEWIRE_USE_STRONG_PULLUP
      onewireWrite(&OWD1, testbuf, 2, MS2ST(750));
#else
      /* poll bus waiting ready signal from all connected devices */
      testbuf[0] = 0;
      while (testbuf[0] == 0){
        osalThreadSleepMilliseconds(50);
        onewireRead(&OWD1, testbuf, 1);
      }
#endif

        /* read temperature device by device from their scratchpads */
        presence = onewireReset(&OWD1);
        osalDbgCheck(true == presence);

        testbuf[0] = ONEWIRE_CMD_SKIP_ROM;
        //memcpy(&testbuf[1], &rombuf[0], 8);
        onewireWrite(&OWD1, testbuf, 1, 0);
        testbuf[0] = ONEWIRE_CMD_READ_SCRATCHPAD;
        onewireWrite(&OWD1, testbuf, 1, 0);
        onewireRead(&OWD1, rombuf, 9);
        osalDbgCheck(rombuf[8] == onewireCRC(rombuf, 8));

        chThdSleepMilliseconds(100);

        presence = onewireReset(&OWD1);
        osalDbgCheck(true == presence);



        tmp = 0;
        //tmp |=  rombuf[3];
        //temperature = tmp ;
        tmp |= (rombuf[1] << 8) | rombuf[0];
        temperature = (tmp * 625) / 10;
    }
    else {
      osalSysHalt("No devices found");
    }

    osalThreadSleep(1); /* enforce ChibiOS's stack overflow check */
    x = 0;
  }

  onewireStop(&OWD1);
  return temperature;
}



