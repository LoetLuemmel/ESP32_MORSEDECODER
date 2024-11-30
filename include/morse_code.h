#pragma once
#include <esp_log.h>
#include <cstring>

namespace MorseCode {
    struct MorseSymbol {
        const char* pattern;
        char letter;
    };
    
    static const MorseSymbol MORSE_TABLE[] = {
        // Buchstaben
        {".-", 'A'},
        {"-...", 'B'},
        {"-.-.", 'C'},
        {"-..", 'D'},
        {".", 'E'},
        {"..-.", 'F'},
        {"--.", 'G'},
        {"....", 'H'},
        {"..", 'I'},
        {".---", 'J'},
        {"-.-", 'K'},
        {".-..", 'L'},
        {"--", 'M'},
        {"-.", 'N'},
        {"---", 'O'},
        {".--.", 'P'},
        {"--.-", 'Q'},
        {".-.", 'R'},
        {"...", 'S'},
        {"-", 'T'},
        {"..-", 'U'},
        {"...-", 'V'},
        {".--", 'W'},
        {"-..-", 'X'},
        {"-.--", 'Y'},
        {"--..", 'Z'},
        
        // Zahlen
        {"-----", '0'},
        {".----", '1'},
        {"..---", '2'},
        {"...--", '3'},
        {"....-", '4'},
        {".....", '5'},
        {"-....", '6'},
        {"--...", '7'},
        {"---..", '8'},
        {"----.", '9'},
        
        // Sonderzeichen
        {".-.-.-", '.'},    // Punkt
        {"--..--", ','},    // Komma
        {"..--..", '?'},    // Fragezeichen
        {".----.", '\''},   // Apostroph
        {"-.-.--", '!'},    // Ausrufezeichen
        {"-....-", '-'},    // Bindestrich
        {"-..-.", '/'},     // Schrägstrich
        {"-.-.-.", ';'},    // Semikolon
        {"---...", ':'},    // Doppelpunkt
        {"-...-", '='},     // Gleichheitszeichen
        {".-..-.", '"'},    // Anführungszeichen
        {".-...", '&'},     // Und-Zeichen
        {".--.-.", '@'},    // At-Zeichen
        {"-.--.", '('},     // Öffnende Klammer
        {"-.--.-", ')'},    // Schließende Klammer
        
        {NULL, '\0'}        // Ende-Marker
    };

    // Erweiterte Zeichenliste
    constexpr const char* OUTPUT_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?'!-/;:=\"&@()";
    constexpr uint8_t MAX_MORSE_LENGTH = 8;

    // Funktion zur Dekodierung eines Morse-Symbols
    inline void decode(const char* symbol) {
        static const char* TAG = "MORSE";
        
        // Sicherheitscheck
        if (!symbol || strlen(symbol) > MAX_MORSE_LENGTH) {
            ESP_LOGI(TAG, "Invalid symbol length");
            return;
        }
        
        // Durch die MORSE_TABLE iterieren
        for(const auto& entry : MORSE_TABLE) {
            if(entry.pattern && strcmp(symbol, entry.pattern) == 0) {
                ESP_LOGI(TAG, "Decoded: '%c' (%s)", entry.letter, symbol);
                return;
            }
        }
        ESP_LOGI(TAG, "Unknown symbol: %s", symbol);
    }

    // Funktion zur Dekodierung eines Morse-Symbols zu einem Zeichen
    inline char decode_to_char(const char* symbol) {
        static const char* MORSE_TAG = "MORSE_DECODE";
        
        // Sicherheitscheck
        if (!symbol || strlen(symbol) > MAX_MORSE_LENGTH) {
            return '\0';
        }
        
        // Durch die MORSE_TABLE iterieren
        for(const auto& entry : MORSE_TABLE) {
            if(entry.pattern && strcmp(symbol, entry.pattern) == 0) {
                char result = entry.letter;
                // Kleinbuchstaben in Großbuchstaben umwandeln
                if (result >= 'a' && result <= 'z') {
                    result = result - 'a' + 'A';
                }
                ESP_LOGI(MORSE_TAG, "Decoded: '%c' (%s)", result, symbol);
                return result;
            }
        }
        return '\0';
    }
} 