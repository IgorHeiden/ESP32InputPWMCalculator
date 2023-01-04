/**
 * ESP32 PWM meter
 * @brief: this source contains all the necessary routines to capture an external
 * PWM signal frequency and duty cycle. It uses the MCPWM's input capture peripheral to do so.
 * Built using ESP-IDF 4.4.5
 * @author: Igor Becker
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_check.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"

const static char *TAG = "PWM-Meter";

#define PWM_PIN    GPIO_NUM_25

/**
 * @brief Globals used during the calculations
 */
volatile uint32_t pos_edg_0 = 0;
volatile uint32_t pos_edg_1 = 0;
volatile uint32_t neg_edg_0 = 0;
float ticks2us;


volatile uint8_t time_2_print = 0; //periodic timer flag, used to print the PWM info on the console


/**
 * @brief struct to hold the calculated frequency and duty cycle
 */
typedef struct {
    uint32_t period;
    uint32_t duty_cycle;
} PWM_Params;


volatile    PWM_Params pwm={0,0};


/**
 * @brief this is the PWM edge transition ISR callback
 */
static void IRAM_ATTR Ext_PWM_ISR_handler(mcpwm_unit_t mcpwm, mcpwm_capture_channel_id_t cap_sig, const cap_event_data_t *edata, void *arg) {
    if (edata->cap_edge == MCPWM_POS_EDGE) {
		pos_edg_1 = edata->cap_value;
		pwm.period = pos_edg_1 - pos_edg_0;
		pos_edg_0 = pos_edg_1;  
    } else { //falling edge
		neg_edg_0 = edata->cap_value;
		pwm.duty_cycle = neg_edg_0 - pos_edg_0;
    }
}

/**
 * @brief this is the peiodic timer ISR, here we flag it's time to print the info
 */
static void IRAM_ATTR print_adcpwm(void* args)
{
    time_2_print = 1;
}


void app_main(void) {
    ticks2us = 1000000.0 / rtc_clk_apb_freq_get(); //calculates the APB bus' clock tick period in micro-secs.    


    ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_0, PWM_PIN)); //Initializes the MCPWM periph...
	
    ESP_ERROR_CHECK(gpio_pulldown_en(PWM_PIN)); //Enables the pull down on the PWM pin to reduce noise...
	
    mcpwm_capture_config_t conf = { //configures the capture unit's parameters
        .cap_edge = MCPWM_BOTH_EDGE,
        .cap_prescale = 1,
        .capture_cb = Ext_PWM_ISR_handler,
        .user_data = NULL };
	
    ESP_ERROR_CHECK(mcpwm_capture_enable_channel(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, &conf)); //set the capture config. parameters and enables the capture unit...
    ESP_LOGI(TAG, "Configuration successful");

    const esp_timer_create_args_t periodic_timer_args = { //configure the periodic timer's parameters, used to display the info on the console
        .callback = &print_adcpwm,
        .name = "periodic" };

	//create and start the timer
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 3000000));

    while (true) {
        if(time_2_print) { //prints the PWM info
            uint32_t duty = (uint32_t) pwm.duty_cycle * ticks2us;
            uint32_t per = (uint32_t) pwm.period * ticks2us;
            ESP_LOGI(TAG, "Duty Cycle: %uus, Period: %uus", duty, per);
            time_2_print=0;
        }
    }
}
