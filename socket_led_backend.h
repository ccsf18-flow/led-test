#ifndef _SOCKET_LED_BACKEND_H_
#define _SOCKET_LED_BACKEND_H_

#include "socket_handle.hpp"

#define TICK_MILLISECONDS 10

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t running;

extern void led_setup();
extern void led_tick_leds(struct msg * new_message, size_t tick);
extern void toggle_relays();
extern void led_shutdown();

#ifdef __cplusplus
}
#endif

#endif
