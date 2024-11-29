#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

static const char* TAG = "ADC_TEST";

// ADC Konfiguration
#define ADC_CHANNEL     ADC_CHANNEL_0  // GPIO 36 (VP)
#define DC_OFFSET       1890           // Gemessener Ruhewert
#define THRESHOLD       100            // Schwelle für Signalerkennung

extern "C" void app_main(void) {
    // ADC Initialisierung
    adc_oneshot_unit_init_cfg_t init_config = {
        ADC_UNIT_1,               // unit_id
        ADC_RTC_CLK_SRC_DEFAULT,  // clk_src
        ADC_ULP_MODE_DISABLE      // ulp_mode
    };
    
    adc_oneshot_chan_cfg_t config = {
        ADC_ATTEN_DB_12,         // atten
        ADC_BITWIDTH_DEFAULT     // bitwidth
    };
    
    adc_oneshot_unit_handle_t adc1_handle;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));
    
    bool last_state = false;
    
    while(1) {
        int adc_raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &adc_raw));
        
        // Prüfe ob Signal vom DC-Offset abweicht
        bool signal_present = (abs(adc_raw - DC_OFFSET) > THRESHOLD);
        
        // Nur ausgeben wenn sich der Zustand ändert
        if (signal_present != last_state) {
            ESP_LOGI(TAG, "%s (ADC: %d, Diff: %d)", 
                    signal_present ? "SIGNAL" : "NON", 
                    adc_raw, 
                    adc_raw - DC_OFFSET);
            last_state = signal_present;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));  // Schnelleres Sampling
    }
}
