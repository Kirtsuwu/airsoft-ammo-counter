#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include"freertos/task.h"
#include "driver/gpio.h"
#include "hd44780.h"
#include <esp_adc/adc_oneshot.h>

#define RESET_BUTTON_PIN 6
#define ADC_PIN ADC_CHANNEL_6
#define ADC_UNIT ADC_UNIT_1
#define ADC_BITWIDTH ADC_BITWIDTH_12   // 12-bit resolution (0-4095)
#define ADC_ATTEN ADC_ATTEN_DB_12      // ~3.3V full-scale voltage

const int max_ammo = 135;
int current_ammo = 0;



//
void microphone_value(void *pvParameter)
{
    int adc_value;
    adc_oneshot_unit_handle_t adc_handle;

    // Initialize ADC Oneshot Mode Driver on the ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_PIN, &config));

    // ADC Oneshot Analog Read loop
    while (1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_PIN, &adc_value));
        if (adc_value > 2500)
        {
            current_ammo = current_ammo - 1;
        }
        printf("ADC Value %d\n", adc_value);
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}
//

void progress_bar(hd44780_t *lcd)
{
    int bar_width=14;
    int filled = (current_ammo * bar_width) / max_ammo;
    
    filled = (current_ammo * bar_width) / max_ammo;
    hd44780_gotoxy(lcd, 0, 1);
    hd44780_putc(lcd, '[');
    for(int i=0; i < bar_width; i++)
    {
        if (i<filled)
        {
             hd44780_putc(lcd, '\x08');
        }
        else
        {
            hd44780_putc(lcd, '-');
        }
    }
    hd44780_putc(lcd, ']');
}


void clear_display(hd44780_t *lcd)
{
    hd44780_gotoxy(lcd, 0, 0);
    hd44780_puts(lcd, "                ");
    hd44780_gotoxy(lcd, 0, 1);
    hd44780_puts(lcd, "                ");
    hd44780_gotoxy(lcd, 0, 0);
}


static const uint8_t ammo_symbol[] = 
{
    0x00, 0x04, 0x0E, 0x0E, 0x0E, 0x00, 0x0E, 0x00 //https://maxpromer.github.io/LCD-Character-Creator/
};



void reset_button_detect(void *pvParameter)
{
    while(1)
    {
        int level = gpio_get_level(RESET_BUTTON_PIN);
        if (level == 1)
        {
            printf("reset button pressed\n");
            current_ammo = max_ammo;
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void display(void *pvParameter)
{
    hd44780_t lcd =
    {
        .write_cb = NULL,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins =
        {
            .rs = GPIO_NUM_13,
            .e  = GPIO_NUM_12,
            .d4 = GPIO_NUM_1,
            .d5 = GPIO_NUM_2,
            .d6 = GPIO_NUM_3,
            .d7 = GPIO_NUM_4,
            .bl = HD44780_NOT_USED
        }
    };

    ESP_ERROR_CHECK(hd44780_init(&lcd));

    hd44780_clear(&lcd);
    hd44780_upload_character(&lcd, 0, ammo_symbol);    //uploading custom symbol to lcd

    // INTRO SEQUENCE

    hd44780_gotoxy(&lcd, 0, 0);

    hd44780_puts(&lcd, "  AIRSOFT AMMO  ");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "    COUNTER    ");
    vTaskDelay(pdMS_TO_TICKS(2000));

    clear_display(&lcd);

    vTaskDelay(pdMS_TO_TICKS(700));

    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "Made by kirtsuwu");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "VER 0.7.0");

    vTaskDelay(pdMS_TO_TICKS(2000));

    clear_display(&lcd);

    vTaskDelay(pdMS_TO_TICKS(700));

    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "AMMO: ");

    char s_current_ammo[16]; // for formatting int to str

    // START MAIN LOOP
    while (1)
    {
        hd44780_gotoxy(&lcd, 7, 0);
        snprintf(s_current_ammo, sizeof(s_current_ammo), "%-3d", current_ammo);
        if (current_ammo > 0) 
        {
            hd44780_puts(&lcd, s_current_ammo); 
        }
        else
        {
            hd44780_puts(&lcd, "OUT");
        }
        progress_bar(&lcd);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void app_main(void) {
    gpio_config_t reset_button_config =
    {
        .pin_bit_mask = (1ULL << RESET_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&reset_button_config);


    xTaskCreate(reset_button_detect, "reset_button_detect", 2048, NULL, 10, NULL);
    xTaskCreate(microphone_value, "microphone_value", configMINIMAL_STACK_SIZE * 3, NULL, 7, NULL);
    xTaskCreate(display, "display", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}





