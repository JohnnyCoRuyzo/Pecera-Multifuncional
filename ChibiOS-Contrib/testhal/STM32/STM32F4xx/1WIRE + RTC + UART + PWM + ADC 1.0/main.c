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

#include "ch.h"
#include "hal.h"


#include "onewire_test.h"


//Variables de Temperatura
int32_t temp = 0, temp1, temp2;
int p1, p2, p3;
char *x = "0";

//Variables de Dosificación
static RTCDateTime timespec;
static RTCDateTime tiempo;
char *pcharb, *pcharh, *pcharm;
int8_t c2,r2;
int8_t k1, k2, k3, k4, k5, rhora, rfrec, rgram, rmin, chora, cmin;
struct tm timx = {0, 53, 17, 56, 4, 60, 3, 0, 0};;

//Variables de Iluminación
//static int A=1;
static char buffer[18];
char *pcharr, *pcharg, *pcharz;
uint16_t rrojo, rverde, razul,aux;

//Variables de ph.
static int32_t mean;
//static bool flag = FALSE;
static float lastvalue, pH;


/*
 * Configura la estructura del UART.
 */

static UARTConfig uart_cfg_1 = {
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  9600,
  0,
  USART_CR2_LINEN,
  0
};


/*
 * Configuración del PWM.
 */


static void pwmpcb(PWMDriver *pwmp) {
  (void)pwmp;
    if(rrojo)
      palClearPad(GPIOB, 13);
    if(rverde)
      palClearPad(GPIOB, 14);
    if(razul)
      palClearPad(GPIOB, 15);
}

static void pwmc1cb(PWMDriver *pwmp) {

  (void)pwmp;
  palSetPad(GPIOB, 13);
}

static void pwmc2cb(PWMDriver *pwmp) {

  (void)pwmp;
  palSetPad(GPIOB, 14);
}

static void pwmc3cb(PWMDriver *pwmp) {

  (void)pwmp;
  palSetPad(GPIOB, 15);
}
	

static PWMConfig pwmcfg = {
  1000000,                                    /* 1MHz PWM clock frequency.   */
  10000,                                       /* Initial PWM period 10ms.   */
  pwmpcb,
  {
   {PWM_OUTPUT_ACTIVE_HIGH, pwmc1cb},
   {PWM_OUTPUT_ACTIVE_HIGH, pwmc2cb},
   {PWM_OUTPUT_ACTIVE_HIGH, pwmc3cb},
   {PWM_OUTPUT_DISABLED, NULL}
  },
  0,
  0
};

/*
 * Declaración de buffers ADC
 */
#define MY_NUM_CH                                              1
#define MY_SAMPLING_NUMBER                                    10

static adcsample_t sample_buff[MY_NUM_CH * MY_SAMPLING_NUMBER];

/*
 * Grupo de Conversión del ADC
 */

static const ADCConversionGroup my_conversion_group = {
  FALSE,                            /*NOT CIRCULAR*/
  MY_NUM_CH,                        /*NUMB OF CH*/
  NULL,                             /*NO ADC CALLBACK*/
  NULL,                             /*NO ADC ERROR CALLBACK*/
  0,                                /* CR1 */
  ADC_CR2_SWSTART,                  /* CR2 */
  0,                                /* SMPR1 */
  ADC_SMPR2_SMP_AN0(ADC_SAMPLE_3),  /* SMPR2 */
  ADC_SQR1_NUM_CH(MY_NUM_CH),       /* SQR1 */
  0,                                /* SQR2 */
  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN0)   /* SQR3 */
};

/*
 * Tarea para el coeficiente de PH.
 */


static THD_WORKING_AREA(waThd1, 512);
static THD_FUNCTION(Thd1, arg) {
  unsigned ii;
  (void) arg;
  chRegSetThreadName("ADC handler");
  /*
   * Activa el ADC.
   */
  adcStart(&ADCD1, NULL);
  while(TRUE) {
    adcConvert(&ADCD1, &my_conversion_group, sample_buff, MY_SAMPLING_NUMBER);

    /* Haciendo el promedio de los valores del ADC.*/
    mean = 0;
    for (ii = 0; ii < MY_NUM_CH * MY_SAMPLING_NUMBER; ii++) {
      mean += sample_buff[ii];
    }
    mean /= MY_NUM_CH * MY_SAMPLING_NUMBER;
    lastvalue = (float)mean * 3.3 / 4096;

    /* pH */
    pH = lastvalue * 4.24;
  }
}



/*
 * Tarea PWM para la iluminación.
 */

static THD_WORKING_AREA(waThd2, 256);
static THD_FUNCTION(Thd2, arg) {


  (void) arg;
  chRegSetThreadName("Led handler");
  pwmStart(&PWMD1, &pwmcfg);



  while(TRUE) {            
	      aux = rrojo*39.2;
	      pwmEnableChannel(&PWMD1, 0, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, aux));
	      pwmEnableChannelNotification(&PWMD1, 0);
	      pwmEnablePeriodicNotification(&PWMD1);
              chThdSleepMilliseconds(10);
	      aux= rverde*39.2;
	      pwmEnableChannel(&PWMD1, 1, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, aux));
	      pwmEnableChannelNotification(&PWMD1, 1);
	      pwmEnablePeriodicNotification(&PWMD1);
	      chThdSleepMilliseconds(10);
	      aux=razul*39.2;
	      pwmEnableChannel(&PWMD1, 2, PWM_PERCENTAGE_TO_WIDTH(&PWMD1, aux));
	      pwmEnableChannelNotification(&PWMD1, 2);
	      pwmEnablePeriodicNotification(&PWMD1);
              chThdSleepMilliseconds(10);
	     		//chThdSleepMilliseconds(10);
			//uartStartSend(&UARTD1, 1, "p");
  }
}


/*
 * Obtiene el tiempo del RTC 
 */
static void GetTimeTm(struct tm *timp) {
  rtcGetTime(&RTCD1, &timespec);
  rtcConvertDateTimeToStructTm(&timespec, timp, NULL);
}


char *numero(int m ){
	char *p;
	switch(m){
	case 1: memcpy(&p, "1", 1);break;
	case 2: memcpy(&p, "2", 1);break;
	case 3: memcpy(&p, "3", 1);break;
	case 4: memcpy(&p, "4", 1);break;
	case 5: memcpy(&p, "5", 1);break;
	case 6: memcpy(&p, "6", 1);break;
	case 7: memcpy(&p, "7", 1);break;
	case 8: memcpy(&p, "8", 1);break;
	case 9: memcpy(&p, "9", 1);break;
	case 0: memcpy(&p, "0", 1);break;
	}
	return p;
}


/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();


  /* switch off wakeup */
  rtcSTM32SetPeriodicWakeup(&RTCD1, NULL);


  palSetPadMode(GPIOA, 0, PAL_MODE_INPUT_ANALOG);

  palSetPadMode(GPIOA, 9, PAL_MODE_ALTERNATE(7));
  palSetPadMode(GPIOA, 10, PAL_MODE_ALTERNATE(7));

  palSetGroupMode(GPIOB, PAL_PORT_BIT(13) | PAL_PORT_BIT(14) | PAL_PORT_BIT(15), 0, PAL_MODE_OUTPUT_PUSHPULL);

  chThdCreateStatic(waThd2, sizeof(waThd2), NORMALPRIO , Thd2, NULL);
  chThdCreateStatic(waThd1, sizeof(waThd1), NORMALPRIO + 1, Thd1, NULL);
	
		      		palClearPad(GPIOA, GPIOA_LED_GREEN);


  while (TRUE) {


	uartStart(&UARTD1, &uart_cfg_1);
	uartStopReceive(&UARTD1);
	uartStopSend(&UARTD1);
	uartStartReceive(&UARTD1, 18, buffer);



	//////*********************//////
	//////****DOSIFICACIÓN*****//////
	//////*********************//////


		chThdSleepMilliseconds(10);
	     	if(strncmp(buffer,"D",1) == 0){

		      	palClearPad(GPIOA, GPIOA_LED_GREEN);

			pcharh = strchr(buffer, 'h');
			pcharm = strchr(buffer, 'm');
			pcharr = strchr(buffer, 'o');
			pcharg = strchr(buffer, 'f');
			pcharb = strchr(buffer, 'g');
			pcharz = strchr(buffer, 'z');

			if(pcharr != 0 && pcharg != 0 && pcharb != 0 && pcharz != 0 && pcharh != 0 && pcharm != 0){

				k1=strcspn(buffer,"m")-strcspn(buffer,"h")-1;
				k2=strcspn(buffer,"o")-strcspn(buffer,"m")-1;
				k3=strcspn(buffer,"f")-strcspn(buffer,"o")-1;
				k4=strcspn(buffer,"g")-strcspn(buffer,"f")-1;
				k5=strcspn(buffer,"z")-strcspn(buffer,"g")-1;
				/*PRUEBA*/
				/*chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k1, pcharh+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k2, pcharm+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k3, pcharr+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k4, pcharg+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k5, pcharb+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, 1, "\n");*/
				/*PRUEBA*/
				rhora = (-48+(int) (pcharr+1)[0])*10+(-48+(int) (pcharr+1)[1]);
				rmin  = (-48+(int) (pcharr+1)[2])*10+(-48+(int) (pcharr+1)[3]);
				rfrec = (-48+(int) (pcharg+1)[0])*10+(-48+(int) (pcharg+1)[1]);
				rgram = (-48+(int) (pcharb+1)[0]);	
				chora = (-48+(int) (pcharh+1)[0])*10+(-48+(int) (pcharh+1)[1]);
				cmin  = (-48+(int) (pcharm+1)[0])*10+(-48+(int) (pcharm+1)[1]);
			}

			strcpy(buffer,"00000000000000000");	
			//  timx = {segundos, minutos, hora, 32+dia, mes-1, año-1956, dia-semana(dom=0), (no se), (no se)};
			struct tm tim1 = {0, cmin, chora, 56, 4, 60, 3, 0, 0};
			timx = tim1;
			rtcConvertStructTmToDateTime(&timx, 0, &tiempo);
			rtcSetTime(&RTCD1, &tiempo);
	     	}

		// Si es el la hora de dosificar, el motor gira la cantidad de gramos que son enviados por el usuario

		GetTimeTm(&timx);
		c2 = rhora/rfrec;
		r2 = rhora-rfrec*(c2);
		if( r2 == timx.tm_hour%(rfrec) && timx.tm_min == rmin && timx.tm_sec == 0){
			palSetPad(GPIOA, GPIOA_LED_GREEN);
		}
		else{
			if(r2 == timx.tm_hour%(rfrec)  && timx.tm_min == rmin && timx.tm_sec == 4*rgram){
		      		palClearPad(GPIOA, GPIOA_LED_GREEN);
			}
		}



	//////*********************//////
	//////*****ILUMINACION*****//////
	//////*********************//////


		chThdSleepMilliseconds(10);
	     	if(strncmp(buffer,"L",1) == 0){

			pcharr = strchr(buffer, 'r');
			pcharg = strchr(buffer, 'g');
			pcharb = strchr(buffer, 'b');
			pcharz = strchr(buffer, 'z');

			if(pcharr != 0 && pcharg != 0 && pcharb != 0 && pcharz != 0){

				k1=strcspn(buffer,"g")-strcspn(buffer,"r")-1;
				k2=strcspn(buffer,"b")-strcspn(buffer,"g")-1;
				k3=strcspn(buffer,"z")-strcspn(buffer,"b")-1;
				/*PRUEBA*/
				/*chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k1, pcharr+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k2, pcharg+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, k3, pcharb+1);
				chThdSleepMilliseconds(10);
				uartStartSend(&UARTD1, 1, "\n");*/
				/*PRUEBA*/
				rrojo =  255-(-48+(int) (pcharr+1)[0])*100+(-48+(int) (pcharr+2)[0])*10+(-48+(int) (pcharr+3)[0]) ;
				rverde = 255-(-48+(int) (pcharg+1)[0])*100+(-48+(int) (pcharg+2)[0])*10+(-48+(int) (pcharg+3)[0]) ;
				razul =  255-(-48+(int) (pcharb+1)[0])*100+(-48+(int) (pcharb+2)[0])*10+(-48+(int) (pcharb+3)[0]) ;
			
			}
				strcpy(buffer,"00000000000000000");
				
			//strcpy(buffer,"Tzzzzzzzzzzzzzzzz");
		}



	//////*********************//////
	//////*****TEMPERATURA*****//////
	//////*********************//////

		chThdSleepMilliseconds(10);
	  	temp = onewireTest();
	  	temp1=temp/1000;
	  	temp2=temp-temp1*1000;
	  
	 	p1 =temp1/10;
	 	p2 =temp1-p1*10;
	 	p3 =temp2/100;


     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, "t");

     		chThdSleepMilliseconds(10);
		x=numero(p1);
		uartStartSend(&UARTD1, 1, &x);

		x=numero(p2);
     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, &x);

     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, ".");

		x=numero(p3);
     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, &x);



	//////*********************//////
	//////***COEFICIENTE PH****//////
	//////*********************//////

		chThdSleepMilliseconds(10);

		p1 = pH;
		p2 = (int) (pH*10) - ((int) p1)*10;

     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, "p");

     		chThdSleepMilliseconds(10);
		x=numero(p1);
		uartStartSend(&UARTD1, 1, &x);

     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, ".");

		x=numero(p2);
     		chThdSleepMilliseconds(10);
		uartStartSend(&UARTD1, 1, &x);


    
    chThdSleepMilliseconds(350);
  }
  return 0;
}
