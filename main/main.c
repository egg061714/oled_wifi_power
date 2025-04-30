//選單基本程式

#include "SSD1306.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include <string.h>
#include "driver/adc.h"
#include "esp_log.h"
#include "image.h"        //icon儲存地
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "protocomm_security.h"
#include "esp_bt.h"


#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_FREQ_HZ 400000
#define OLED_ADDR 0x3C
#define THRESHOLD 30
#define button GPIO_NUM_19

#define SSID "wifi_ap"
#define PASSWORD "12345678"
#define bluename "PROV_ESP32"

static const char *TAG = "MY_AP_MODE";
int8_t powerset;

char buf[20];
char vr[20];
int gamemode=1;
int flash[2];
int adc_reading;
int level=1;
int allmode;
bool is_running=false;
bool indo = false;
bool is_ble_initialized = false;  // ← 新增的狀態追蹤變數
bool blu_open=false;
bool prov;
SSD1306_t dev;
wifi_config_t wifi_config;


/* ------- 檔案頂端，全域常量 -------- */
wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
const char *pop = "abcd1234";
const char *service_name = bluename;
const char *service_key = NULL;
int map(int x, int in_min, int in_max, int out_min, int out_max);

void reset_wifi_config() {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open("nvs.net80211", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI("RESET", "✅ Wi-Fi 設定已清除！裝置將重新啟動...");
        vTaskDelay(pdMS_TO_TICKS(500)); // 等一點時間讓log送出
        esp_restart(); // 🔥 這行很重要！重開才會真的"忘記"配網狀態
    } else {
        ESP_LOGE("RESET", "⚠️ 無法開啟 Wi-Fi NVS，錯誤代碼: %s", esp_err_to_name(err));
    }
}

static void wifi_event_handler_sta(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "✅ Wi-Fi 連線成功!");
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGE(TAG, "⚠️ Wi-Fi 斷線，重新嘗試...");
        esp_wifi_connect();
    }
}
static void prov_event_handler(void *user_data, wifi_prov_cb_event_t event, void *event_data) {
    switch (event) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "📡 BLE Provisioning 開始...");
            break;
        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "📥 接收到 Wi-Fi 設定 -> SSID: %s, 密碼: %s",
                     (const char *)wifi_sta_cfg->ssid,
                     (const char *)wifi_sta_cfg->password);
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "✅ Provisioning 成功");
            break;
        case WIFI_PROV_END:
            ESP_LOGI(TAG, "⚡ Provisioning 結束");
            
            is_ble_initialized = false;  // ← BLE 關閉時重設狀態
            break;
        default:
            break;
    }
}

void final_mode() {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    if(blu_open==false)
    {
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&prov));  // <<== 加上這行！正確讀取狀態

    if (!prov) {
        ESP_LOGI(TAG, "🔵 沒憑證，啟動 BLE 配對...");
        if (!is_ble_initialized) {
            wifi_prov_mgr_config_t config = {
                .scheme = wifi_prov_scheme_ble,
                .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
            };
            ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
            is_ble_initialized = true;
        }
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key));
        blu_open = true;
    } else {
        ESP_LOGI(TAG, "📶 已有憑證，直接連線 Wi-Fi");
        // 這邊不用再 set_mode 和 start了，因為上面已經 start Wi-Fi
    }
    }
}


static void wifi_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station connected - MAC: " MACSTR ", AID=%d",
                         MAC2STR(event->mac), event->aid);
                wifi_sta_list_t sta_list;
                if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
                    for (int i = 0; i < sta_list.num; i++) {
                        wifi_sta_info_t station = sta_list.sta[i];
                        ESP_LOGI(TAG, "Station " MACSTR " RSSI = %d dBm", MAC2STR(station.mac), station.rssi);
                    }
                }
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station disconnected - MAC: " MACSTR ", AID=%d",
                         MAC2STR(event->mac), event->aid);
                break;
            }
            default:
                break;
        }
    }
}



void nvs_init()
{
    // 初始化 NVS（用於儲存 Wi-Fi 設定等）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}


void wifi_egg_init()
{
      

  
      // 初始化網路介面及事件迴圈
      ESP_ERROR_CHECK(esp_netif_init());
      ESP_ERROR_CHECK(esp_event_loop_create_default());
  
      // 建立預設的 AP 網路介面
      esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
      esp_netif_create_default_wifi_sta();
      // Wi-Fi 初始化
      wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
      ESP_ERROR_CHECK(esp_wifi_init(&cfg));
      
      
      // 註冊 Wi-Fi 事件處理器
      ESP_ERROR_CHECK(esp_event_handler_instance_register(
              WIFI_EVENT,
              ESP_EVENT_ANY_ID,
              &wifi_event_handler,
              NULL,
              NULL));

              ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_sta, NULL));
              ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_sta, NULL));
          
      




}
void wifi_power()
{

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  
    // 設定 AP 參數 (SSID, 密碼, 信道, max_connection, authmode 等)
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = SSID,               // AP SSID
            .ssid_len = strlen(SSID),
            .password = PASSWORD,       // AP 密碼
            .channel = 1,               // 可自行更改
            .max_connection = 4,        // 可連線之裝置數
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
   
       ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
       ESP_ERROR_CHECK(esp_wifi_start());
       int power=map(adc_reading ,0,511,8,80);
       ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(power));
       if(power<10)
        ssd1306_clear_screen(&dev, false);
       sprintf(vr, "%d",  power);
       ssd1306_display_text(&dev, 0, vr, strlen(vr), false);
   
       ESP_LOGI(TAG, "Wi-Fi AP init done. SSID:%s Password:%s Channel:%d",
                wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);
   
       ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&powerset));
       ESP_LOGI(TAG, "初始 Tx Power：%.2f dBm", powerset * 0.25);
       sprintf(buf, "%f",  powerset * 0.25);
       ssd1306_display_text(&dev, 4, buf, strlen(buf), false);

}

void vrx()
{
    adc1_config_width(ADC_WIDTH_BIT_9);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc_reading = adc1_get_raw(ADC1_CHANNEL_6);
    printf("%d",adc_reading);
    if(indo!=true)
    {
        static int last_value = 0;
        int diff = adc_reading - last_value;
        if (abs(diff) > THRESHOLD) {
            if (diff > 0) {
                gamemode++;
                if (gamemode > 5) gamemode = 1;
            } else {
                gamemode--;
                if (gamemode < 1) gamemode = 5;
            }
            last_value = adc_reading;
        }
    }


    vTaskDelay(pdMS_TO_TICKS(100));
}
void button_state(int button)
{
    gpio_reset_pin(button);
    gpio_set_direction(button, GPIO_MODE_INPUT);


            level = gpio_get_level(button);
            printf("GPIO2 Level: %d\n", level);
            // sprintf(buf, "%d", level);
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
void doing(int allmode)
{
    switch(allmode)
    {
    case 1:
    if (level == 0) {
        is_running = false;  // 結束當前模式
        allmode = 0;
        indo=false;

        ssd1306_clear_screen(&dev, false);
        ESP_ERROR_CHECK(esp_wifi_stop());

    }else
    {
      wifi_power();  
    }
    break;

    case 4:
    if (level == 0) {
        is_running = false;  // 結束當前模式
        allmode = 0;
        indo=false;
        blu_open = true;  // 🔒 不能連 BLE
        ssd1306_clear_screen(&dev, false);


    
        is_ble_initialized=true;
        ESP_ERROR_CHECK(esp_wifi_stop());

    }else
    {
        printf("game over");
        final_mode();
    }   
        break;
    
    default:
        break;
    }
}
void mode(int gamemode)
{
    switch (gamemode)
    {
    case 1:
    
    if( is_running!= true)
    {
        show_wifi_icon(&dev);
        if(level==0)
        {
            ssd1306_clear_screen(&dev, false);
            allmode=1;
            is_running=true;
            indo=true;

        }
    }else
    {
        doing(allmode);
    }
    
        break;
    case 2:
    show_blu_icon(&dev);
        break;
    case 3:
    show_lig_icon(&dev);
        break;
    case 4:
    
    if( is_running!= true)   //表示還沒觸發
    {
        show_cycu_logo(&dev);
        if(level==0)
        {
            ssd1306_clear_screen(&dev, false);
            allmode=4;
            is_running=true;
            indo=true;
            blu_open = false;

            
        }
    }else
    {
        doing(allmode);
    }
        break;    
        
    case 5:    //重置mode
    
        show_reset_logo(&dev);
            if(level==0)
            {
                ssd1306_clear_screen(&dev, false);
                reset_wifi_config();
            }

    break;  
    default:
        break;
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_bt_mem_release(ESP_BT_MODE_CLASSIC_BT));
    nvs_init();//初始化nvs
    oled_config_egg_init();
    wifi_egg_init();

    while(1)
    {
        button_state(button);
        vrx();
        mode(gamemode);

        // ssd1306_display_text(&dev, 0, buf, strlen(buf), false);
        // ssd1306_display_text(&dev, 1, vr, strlen(vr), false);
    }




    
}




int map(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
