#include <stdio.h>
#include <stdlib.h> // Required for abs()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_mic.h" 

#define SAMPLE_BUFFER_SIZE 256

void app_main(void) {
    printf("Initializing Privacy Shield...\n");
    mic_init();
    printf("Microphone initialized successfully!\n");

    int32_t sample_buffer[SAMPLE_BUFFER_SIZE];
    size_t bytes_read;
    int print_counter = 0; // Used to slow down the terminal output

    while (1) {
        esp_err_t err = i2s_channel_read(rx_chan, sample_buffer, sizeof(sample_buffer), &bytes_read, portMAX_DELAY);

        if (err == ESP_OK) {
            int samples_read = bytes_read / sizeof(int32_t);

            if (samples_read > 0) {
                // 1. Calculate the average loudness of this chunk
                int64_t total_energy = 0; 
                for (int i = 0; i < samples_read; i++) {
                    // Turn all negative dips in the sound wave into positive peaks
                    int32_t sample_val = sample_buffer[i];
                    if (sample_val < 0) sample_val = -sample_val; 
                    
                    total_energy += sample_val;
                }
                
                int32_t average_volume = total_energy / samples_read;

                // 2. Slow down the printing!
                // We read chunks incredibly fast. Let's only print every 10th chunk.
                print_counter++;
                if (print_counter >= 10) {
                    int bars = average_volume / 5000; 
                    if (bars > 50) bars = 50; // Cap it so it doesn't wrap around the screen

                    printf("Vol [%8ld] | ", average_volume);
                    for (int i = 0; i < bars; i++) {
                        printf("#"); // Print a bar for the volume
                    }
                    printf("\n");
                    
                    print_counter = 0;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}