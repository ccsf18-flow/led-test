#include "socket_handle.cpp"
#include "socket_led_backend.h"

#include <chrono>
#include <iostream>

using std::chrono::steady_clock;
using std::chrono::milliseconds;

int main(){
  printf("Starting socket connection... \n");
  struct server_socket server_socket = begin_recv();
  int ret;
  led_setup();

  auto next_tick = steady_clock::now() + milliseconds(TICK_MILLISECONDS);
  // std::cout << "next tick at " << next_tick;
  size_t tick = 0;
  while(running){
    msg rec_msg = recv_data(&server_socket);

    if (rec_msg.tag != MSG_TAG_NO_MSG) {
      printf("Value recieved!\n");
      printf("Message tag: %d\n", rec_msg.tag);
      printf("Message time: %d\n", rec_msg.time_in_ms);
      printf("Message Data...\n");
      printf("\tNote: %d\n", rec_msg.data_msg.b_msg.note);
      led_tick_leds(&rec_msg, tick);
    }

    auto current_time = steady_clock::now();
    if (next_tick < current_time) {
      led_tick_leds(&rec_msg, tick);

      next_tick = current_time + milliseconds(TICK_MILLISECONDS);
    // std::cout << "next tick at " << next_tick;
      tick++;
    }
  }
  led_shutdown();

  return 0;
}
