#include <esp_adc/adc_continuous.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_pm.h"
#include "esp_system.h"
#include "morse_code.h"

#define SAMPLE_BUFFER_SIZE 256

// Morse timing constants (in milliseconds)
#define DIT_LENGTH      100     // Base unit length
#define DAH_LENGTH      300     // 3x DIT length
#define CHAR_SPACE      300     // Space between characters (3x DIT)
#define WORD_SPACE      700     // Space between words (7x DIT)
#define TIMING_TOLERANCE 0.2    // 20% timing tolerance

// ADC and signal detection constants
#define SIGNAL_THRESHOLD    800  // ADC threshold (0-4095)
#define NOISE_THRESHOLD    150   // Noise floor
#define MIN_SAMPLES_FOR_SIGNAL 5 // Minimum samples above threshold to detect signal

// Timing-Konstanten (in Millisekunden)
#define DOT_DURATION 100     // Typische Länge eines DIT
#define DASH_DURATION 300    // Typische Länge eines DASH
#define SYMBOL_GAP 100      // Pause zwischen Symbolen
#define LETTER_GAP 300      // Pause zwischen Buchstaben
#define WORD_GAP 700        // Pause zwischen Wörtern

static TaskHandle_t s_task_handle;
static adc_continuous_handle_t adc_handle = NULL;

static bool IRAM_ATTR s_conversion_done_cb(adc_continuous_handle_t handle, 
                                         const adc_continuous_evt_data_t *edata, 
                                         void *user_data) {
    BaseType_t mustYield = pdFALSE;
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
    return (mustYield == pdTRUE);
}

static const char* TAG = "MORSE";

void morse_decoder_task(void* arg) {
    adc_continuous_handle_t adc_handle = (adc_continuous_handle_t)arg;
    
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(SAMPLE_BUFFER_SIZE * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ADC buffer");
        vTaskDelete(NULL);
        return;
    }
    
    // Morse Dekodierung Variablen
    char morse_buffer[MorseCode::MAX_MORSE_LENGTH] = {0};
    int morse_pos = 0;
    uint32_t signal_start = 0;
    uint32_t signal_end = 0;
    bool last_state = false;
    uint32_t last_change = 0;
    
    while (true) {
        uint32_t ret_num = 0;
        esp_err_t ret = adc_continuous_read(adc_handle, buffer, 
                                          SAMPLE_BUFFER_SIZE * sizeof(uint16_t),
                                          &ret_num, 0);
        
        if (ret == ESP_OK) {
            uint16_t* samples = (uint16_t*)buffer;
            uint32_t num_samples = ret_num / sizeof(uint16_t);
            
            uint32_t sum = 0;
            for(uint32_t i = 0; i < num_samples; i++) {
                sum += samples[i] & 0x0FFF;
            }
            uint16_t avg = sum / num_samples;
            
            bool current_state = (avg > 150);
            uint32_t current_time = esp_timer_get_time() / 1000; // Zeit in ms
            
            // Zustandsänderung erkennen
            if (current_state != last_state) {
                if (current_state) { // Signal geht auf HIGH
                    signal_start = current_time;
                } else { // Signal geht auf LOW
                    signal_end = current_time;
                    uint32_t duration = signal_end - signal_start;
                    
                    // DIT oder DASH erkennen
                    if (duration >= DOT_DURATION/2) {
                        if (duration < (DOT_DURATION + DASH_DURATION)/2) {
                            morse_buffer[morse_pos++] = '.';
                        } else {
                            morse_buffer[morse_pos++] = '-';
                        }
                        morse_buffer[morse_pos] = '\0';
                    }
                }
                last_change = current_time;
            } else if (!current_state) { // Während LOW-Phase
                uint32_t gap = current_time - last_change;
                
                // Buchstaben-Ende erkennen
                if (gap > LETTER_GAP && morse_pos > 0) {
                    MorseCode::decode(morse_buffer);
                    morse_pos = 0;
                    morse_buffer[0] = '\0';
                }
                // Wort-Ende erkennen (optional)
                else if (gap > WORD_GAP) {
                    // Nur wenn wir Wort-Trennung wollen:
                    // ESP_LOGI(TAG, " ");
                }
            }
            
            last_state = current_state;
            gpio_set_level(GPIO_NUM_17, current_state);
            
            if (morse_pos >= MorseCode::MAX_MORSE_LENGTH - 1) {
                morse_pos = 0;
                morse_buffer[0] = '\0';
                ESP_LOGW(TAG, "Morse buffer overflow");
            }
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    free(buffer);
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "Initializing Morse decoder...");
    
    // LED Setup (GPIO 17)
    gpio_config_t led_gpio_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_17),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&led_gpio_config));
    
    // Erste Konfiguration für den Handle
    adc_continuous_handle_cfg_t handle_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = SAMPLE_BUFFER_SIZE,
        .flags = {
            .flush_pool = 0
        }
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_config, &adc_handle));

    // ADC Pattern konfigurieren
    adc_digi_pattern_config_t adc_pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = ADC_CHANNEL_0,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12
    };

    // ADC Konfiguration
    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &adc_pattern,
        .sample_freq_hz = 45000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    
    // ADC starten
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    
    // Task erstellen
    TaskHandle_t task_handle;
    BaseType_t ret = xTaskCreate(
        morse_decoder_task,
        "morse_decoder",
        8192,
        (void*)adc_handle,
        5,
        &task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create morse decoder task!");
        return;
    }
    
    ESP_LOGI(TAG, "Morse decoder initialized and running");
}
