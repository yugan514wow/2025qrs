#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_i2c.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "iot_uart.h"
#include "iot_errno.h"
#include <hi_pwm.h>
#include <hi_gpio.h>
#include <hi_io.h>
#include <hi_adc.h>
#include <hi_watchdog.h>

#include "aht20.h"

#define DELAY_US     (2000000)
#define gpio_high1     (6)  
#define gpio_high2     (8)  

float g_Temp = 0.0;    
float g_Humi = 0.0;    
int g_smoke = 0;  
int g_Lumi = 0;   

#define reset    1
#define MAX_PPM  (430)
#define I2C0_IDX      (0)
#define I2C0_BAUDRATE (400*1000)

static int I2C0Init(void)
{
    IoTGpioInit(HI_IO_NAME_GPIO_13);
    IoTGpioInit(HI_IO_NAME_GPIO_14);
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);


    return IoTI2cInit(I2C0_IDX, I2C0_BAUDRATE);
}

static void I2C0DeInit(void)
{
    IoTI2cDeinit(I2C0_IDX);
}

void MainTask(void)
{
    int ret = -1;
    hi_watchdog_disable();

    if (I2C0Init() != 0) {
        return;
    }
    IoTGpioInit(gpio_high1);
    IoTGpioSetDir(gpio_high1, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(gpio_high1, 0);  
    IoTGpioInit(gpio_high2);
    IoTGpioSetDir(gpio_high2, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(gpio_high2, 0);  
    hi_gpio_init();
    hi_io_set_func(HI_GPIO_IDX_5, HI_IO_FUNC_GPIO_5_GPIO);
    hi_gpio_set_dir(HI_GPIO_IDX_5, HI_GPIO_DIR_IN);
    hi_io_set_func(HI_GPIO_IDX_9, HI_IO_FUNC_GPIO_9_GPIO);
    hi_gpio_set_dir(HI_GPIO_IDX_9, HI_GPIO_DIR_IN);

    WifiTask();
    huawei_cloud_mqtt_init();

    ret = AHT20_Calibrate();
    printf("[MainTask] AHT20_Calibrate: ret[%d]\n", ret);

    while (1) {
        sleep(2);
        ret = AHT20_StartMeasure();
        printf("[MainTask] AHT20_StartMeasure: ret[%d]\n", ret);

        ret = AHT20_GetMeasureResult(&g_Temp, &g_Humi);
        printf("[MainTask] AHT20_GetMeasureResult: ret[%d], Temp[%.2f], Humi[%.2f]\n",
               ret, g_Temp, g_Humi);
        if (hi_adc_read(HI_ADC_CHANNEL_2, &g_Lumi,HI_ADC_EQU_MODEL_2, HI_ADC_CUR_BAIS_DEFAULT, 0) != HI_ERR_SUCCESS) {
            printf("guangzhao read error!\n");
        } else {
            printf("guangzhao=%u\n", (unsigned int)g_Lumi);
            usleep(DELAY_US);
        }
        if (hi_adc_read(HI_ADC_CHANNEL_4, &g_smoke, HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0) != HI_ERR_SUCCESS) {
            printf("nongdu read error!\n");
        } else {
            printf("nongdu=%u\n", (unsigned int)g_smoke);
            if(g_smoke>MAX_PPM){
                IoTGpioSetOutputVal(gpio_high1, 1);  
                IoTGpioSetOutputVal(gpio_high2, 1);  
                printf("Beer is ON, Both high level outputs are ON!\n");
            }else{
                IoTGpioSetOutputVal(gpio_high1, 0);  
                IoTGpioSetOutputVal(gpio_high2, 0);  
                printf("Beer is OFF, Both high level outputs are OFF!\n");
            }
            usleep(DELAY_US);
        }
        deal_report_msg(); 
    }
    I2C0DeInit();
}

void MainEntry(void)
{
    osThreadAttr_t attr = {"MainTask", 0, NULL, 0, NULL, 1024*10, 24, 0, 0};

    if (osThreadNew((osThreadFunc_t)MainTask, NULL, &attr) == NULL) {
        printf("[MainEntry] create MainTask Failed!\n");
    }
}
SYS_RUN(MainEntry);
