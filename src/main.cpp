#include "driver/i2s_std.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <u8g2.h>

#define SAMPLE_RATE     5000
#define DMA_BUFFER_SIZE 1024
#define I2S_PORT        I2S_NUM_0

// I2C Pins definieren
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define I2C_FREQ_HZ 400000  // 400kHz

static int16_t dma_buffer[DMA_BUFFER_SIZE];

// Display-Objekt für I2C OLED
u8g2_t u8g2;

// I2C Initialisierung
void i2c_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));
}

// ADC und I2S initialisieren
void adc_dma_init() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = DMA_BUFFER_SIZE / 4,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    // I2S Treiber installieren
    ESP_ERROR_CHECK(i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL));
    
    // ADC1 mit I2S verbinden
    ESP_ERROR_CHECK(i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0));
    ESP_ERROR_CHECK(i2s_adc_enable(I2S_PORT));
}

extern "C" void app_main(void) {
    // I2C initialisieren
    i2c_init();
    
    // Display initialisieren
    u8g2_Init(&u8g2, &u8g2_dev_ssd1306_128x64, u8g2_GetI2CPort(), U8G2_R0, u8g2_GetRotation(), U8X8_PIN_NONE);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_ClearBuffer(&u8g2);
    
    // Test-Muster zeichnen
    u8g2_DrawStr(&u8g2, 0, 10, "Test Display");
    u8g2_DrawBox(&u8g2, 0, 20, 20, 20);
    u8g2_SendBuffer(&u8g2);
    
    // ADC und DMA initialisieren
    adc_dma_init();

    while (1) {
        size_t bytes_read;
        i2s_read(I2S_PORT, (void *)dma_buffer, DMA_BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        // Display aktualisieren
        u8g2_ClearBuffer(&u8g2);
        u8g2_DrawStr(&u8g2, 0, 10, "Morse Signal:");
        
        // Letzte 128 Samples als Grafik anzeigen
        for(int i = 0; i < 128 && i < bytes_read/sizeof(int16_t); i++) {
            int16_t sample = dma_buffer[i] & 0x0FFF;
            int height = map(sample, 0, 4095, 0, 40);
            u8g2_DrawVLine(&u8g2, i, 64-height, height);
        }
        
        u8g2_SendBuffer(&u8g2);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
