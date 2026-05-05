#include "driver/i2s_std.h"
#include "esp_err.h"

#define I2S_BCLK        GPIO_NUM_5
#define I2S_WS          GPIO_NUM_6   // WS (Word Select) is the new name for LRCLK
#define I2S_DIN         GPIO_NUM_4   // Microphone data in

// Global handle for the RX channel
i2s_chan_handle_t rx_chan; 

void mic_init(void) {
    // 1. Allocate a new RX channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // 2. Configure the channel to standard I2S mode (Philips format)
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    // 3. Enable the channel so it starts receiving data
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}