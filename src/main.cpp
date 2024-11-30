#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "font8x8.h"
#include "morse_code.h"

static const char* TAG = "TEST";

// I2C Definitionen
#define I2C_MASTER_SCL_IO           22      // GPIO für I2C SCL
#define I2C_MASTER_SDA_IO           21      // GPIO für I2C SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define OLED_ADDR                   0x3C

// I2C Initialisierung
static esp_err_t i2c_master_init(void) {
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000; // Standard 100KHz I2C Frequenz
    
    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

// OLED Display Initialisierung (Ihre funktionierende Version)
static void oled_init() {
    uint8_t init_cmd[] = {
        0x00,   // Command mode
        0xAE,   // Display off
        0xD5, 0x80,   // Set display clock
        0xA8, 0x3F,   // Set multiplex
        0xD3, 0x00,   // Set display offset
        0x40,   // Start line
        0x8D, 0x14,   // Charge pump
        0x20, 0x00,   // Memory mode
        0xA1,   // Segment remap
        0xC8,   // COM scan direction
        0xDA, 0x12,   // COM pins
        0x81, 0xCF,   // Contrast
        0xD9, 0xF1,   // Pre-charge
        0xDB, 0x40,   // VCOMH
        0xA4,   // Display RAM
        0xA6,   // Normal display
        0xAF    // Display on
    };
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, init_cmd, sizeof(init_cmd), true);
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000)));
    i2c_cmd_link_delete(cmd);
    ESP_LOGI(TAG, "OLED initialized");
}

// Display löschen mit längerem Timeout
static void clear_display() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    
    // Set column address (0-127)
    i2c_master_write_byte(cmd, 0x00, true); // command mode
    i2c_master_write_byte(cmd, 0x21, true); // set column addr
    i2c_master_write_byte(cmd, 0, true);    // start at 0
    i2c_master_write_byte(cmd, 127, true);  // end at 127
    
    // Set page address (0-7)
    i2c_master_write_byte(cmd, 0x22, true); // set page addr
    i2c_master_write_byte(cmd, 0, true);    // start at 0
    i2c_master_write_byte(cmd, 7, true);    // end at 7
    
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000))); // Längerer Timeout
    i2c_cmd_link_delete(cmd);
    
    // Kurze Pause
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Fill with zeros
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true); // data mode
    
    // Fill all pages with zeros
    for (int i = 0; i < 1024; i++) {  // 128 columns * 8 pages
        i2c_master_write_byte(cmd, 0x00, true);
    }
    
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000))); // Längerer Timeout
    i2c_cmd_link_delete(cmd);
}

// Vereinfachte Textausgabe mit rotierten Bytes
static void display_text(const char* text) {
    // Display zuerst löschen
    clear_display();
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    
    // Set column address (0-127)
    i2c_master_write_byte(cmd, 0x00, true); // command mode
    i2c_master_write_byte(cmd, 0x21, true); // set column addr
    i2c_master_write_byte(cmd, 0, true);    // start
    i2c_master_write_byte(cmd, 127, true);  // end
    
    // Set page address (2-3) - mittlere Zeilen
    i2c_master_write_byte(cmd, 0x22, true); // set page addr
    i2c_master_write_byte(cmd, 2, true);    // start at page 2
    i2c_master_write_byte(cmd, 3, true);    // end at page 3
    
    // Ensure normal display mode
    i2c_master_write_byte(cmd, 0xA6, true); // Normal display
    
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000)));
    i2c_cmd_link_delete(cmd);
    
    // Write data
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x40, true); // data mode
    
    // Write each character
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        if (c >= ' ' && c <= 'Z') {  // Nur Großbuchstaben und Sonderzeichen
            const uint8_t* char_data = &font8x8[(c - ' ') * 8];
            for (int j = 0; j < 8; j++) {
                i2c_master_write_byte(cmd, char_data[j], true);
            }
        }
    }
    
    i2c_master_stop(cmd);
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000)));
    i2c_cmd_link_delete(cmd);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initializing I2C");
    ESP_ERROR_CHECK(i2c_master_init());
    
    ESP_LOGI(TAG, "Initializing OLED");
    oled_init();
    
    // Start message
    display_text("MORSE READY");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Test der Morse-Code Dekodierung
    const char* test_patterns[] = {
        "...",   // S
        "---",   // O
        "...",   // S
    };
    
    char decoded_text[16] = "";  // Kleiner Buffer für den dekodierten Text
    int pos = 0;
    
    // Dekodiere jeden Pattern
    for(const char* pattern : test_patterns) {
        char result = MorseCode::decode_to_char(pattern);
        if(result != '\0' && pos < 14) {  // Sicherheitscheck für Buffer
            decoded_text[pos++] = result;
            decoded_text[pos] = '\0';
            
            // Zeige aktuellen Stand an
            char display_buffer[16];  // Kleiner Buffer für die Anzeige
            snprintf(display_buffer, sizeof(display_buffer), "%s", decoded_text);  // Nur Text ohne Prefix
            display_text(display_buffer);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
