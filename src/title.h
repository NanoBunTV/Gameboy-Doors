// The title screen: shows the logo and plays the Lobby Theme, waits for Start, returns the run's seed
#ifndef TITLE_H
#define TITLE_H

#include <gb/gb.h>
#include <stdint.h>

uint16_t title_run(void) BANKED;

#endif
