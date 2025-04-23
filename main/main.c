//選單基本程式

#include "SSD1306.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <string.h>
#include "driver/adc.h"
#include "esp_log.h"
#include "image.h"
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 400000
#define OLED_ADDR 0x3C
#define THRESHOLD 30
#define button GPIO_NUM_19
char buf[20];
char vr[20];
int gamemode=1;
int flash[2];
int adc_reading;
SSD1306_t dev;

void vrx()
{
    static int last_value = 0;

    adc1_config_width(ADC_WIDTH_BIT_9);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc_reading = adc1_get_raw(ADC1_CHANNEL_6);

    int diff = adc_reading - last_value;
    if (abs(diff) > THRESHOLD) {
        if (diff > 0) {
            gamemode++;
            if (gamemode > 4) gamemode = 4;
        } else {
            gamemode--;
            if (gamemode < 1) gamemode = 1;
        }
        last_value = adc_reading;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
}
void button_state(int button)
{
    gpio_reset_pin(button);
    gpio_set_direction(button, GPIO_MODE_INPUT);


            int level = gpio_get_level(button);
            printf("GPIO2 Level: %d\n", level);
            sprintf(buf, "%d", level);
            vTaskDelay(5); // 每500ms讀一次

        
}
void oled_config_egg_init()
{
   // 初始化I2C
   i2c_master_bus_handle_t i2c_bus_handle;
   i2c_master_bus_config_t i2c_bus_config = {
       .clk_source = I2C_CLK_SRC_DEFAULT,
       .i2c_port = I2C_MASTER_NUM,
       .sda_io_num = I2C_MASTER_SDA_IO,
       .scl_io_num = I2C_MASTER_SCL_IO,
       .glitch_ignore_cnt = 7,
       .flags.enable_internal_pullup = true,
   };
   ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));

   i2c_device_config_t dev_cfg = {
       .dev_addr_length = I2C_ADDR_BIT_LEN_7,
       .device_address = OLED_ADDR,
       .scl_speed_hz = I2C_MASTER_FREQ_HZ,
   };
   i2c_master_dev_handle_t i2c_dev_handle;
   ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &i2c_dev_handle));

   // 初始化OLED
   
   dev._address = OLED_ADDR;
   dev._width = 128;
   dev._height = 64;
   dev._pages = 8;
   dev._flip = false;
   dev._i2c_dev_handle = i2c_dev_handle;
   ssd1306_init(&dev, 128, 64);

   // 清螢幕
   ssd1306_clear_screen(&dev, false);

   // 顯示文字
   

}
void mode(int gamemode)
{
    switch (gamemode)
    {
    case 1:
    show_wifi_icon(&dev);
        break;
    case 2:
    show_blu_icon(&dev);
        break;
    case 3:
    show_lig_icon(&dev);
        break;
    case 4:
    show_cycu_logo(&dev);
        break;                    
    default:
        break;
    }
}
void app_main(void)
{
    
    oled_config_egg_init();

    while(1)
    {
        button_state(button);
        vrx();
        mode(gamemode);
        // ssd1306_display_text(&dev, 0, buf, strlen(buf), false);
        // ssd1306_display_text(&dev, 1, vr, strlen(vr), false);
    }




    
}
