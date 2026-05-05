#pragma once

#include "driver/i2s_std.h"

// The 'extern' keyword tells the compiler: "This variable exists, 
// but it is defined in another .c file (i2s_mic.c)."
extern i2s_chan_handle_t rx_chan;

// Declare the initialization function
void mic_init(void);