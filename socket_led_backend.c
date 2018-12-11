#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

#include "rpi_ws281x/clk.h"
#include "rpi_ws281x/gpio.h"
#include "rpi_ws281x/dma.h"
#include "rpi_ws281x/pwm.h"
#include "rpi_ws281x/version.h"

#include "rpi_ws281x/ws2811.h"

#include "socket_led_backend.h"

#include <wiringPi.h>

#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                12
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT               16 
#define LEDS_PER_UNIT            8

uint8_t running = 1;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .invert = 0,
            .count = LED_COUNT,
            .strip_type = STRIP_TYPE,
            .brightness = 255,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .brightness = 0,
        },
    },
};

static void ctrl_c_handler(int signum)
{
  (void)(signum);
  running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

struct color {
  uint8_t r, g, b, a;
};

uint8_t lerp(uint8_t a, uint8_t b, uint8_t f) {
  float fac = f / 255.f;
  float af = a * (1.f - fac);
  float bf = b * fac;
  return (uint8_t)(af + bf);
}

int randint(int l, int h) {
  float f = (float)random() / (float)RAND_MAX;
  return (int)(f * (float)(h - l)) + l;
}

struct color random_color() {
  struct color tr;
#define CMAX 64
  tr.r = randint(0, CMAX);
  tr.g = randint(0, CMAX);
  tr.b = randint(0, CMAX);
  tr.a = 0;
  // tr.a = 0xFF; // 0; // randint(0, 64);
#undef CMAX
  return tr;
}

struct color lerp_color(struct color a, struct color b, uint8_t f) {
  struct color tr;
  tr.r = lerp(a.r, b.r, f);
  tr.g = lerp(a.g, b.g, f);
  tr.b = lerp(a.b, b.b, f);
  tr.a = lerp(a.a, b.a, f);
  return tr;
}

uint32_t intcolor(struct color c) {
  return c.b | ((uint32_t)c.r << 8) | ((uint32_t)c.g << 16) | (((uint32_t)c.a) << 24);
}

uint32_t rotcolor(uint32_t c) {
  return (c & (0xFF << 24)) |
    ((c << 8) & 0xFFFF00) |
    ((c >> 16) & 0xFF);
}

void print_color(struct color c) {
  printf("%08x\n", intcolor(c));
}

static struct color colors[12] = {
  // { .r = 0x10, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x20, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x30, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x40, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x50, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x60, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x70, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x80, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0x90, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0xA0, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0xB0, .g = 0x00, .b = 0x00, .a = 0x00 },
  // { .r = 0xC0, .g = 0x00, .b = 0x00, .a = 0x00 },

  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x00 },
  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x1F }  ,
  { .r = 0x00, .g = 0x1F, .b = 0x00, .a = 0x00 }  ,
  { .r = 0x00, .g = 0x1F, .b = 0x00, .a = 0x1F }  ,
  { .r = 0x00, .g = 0x00, .b = 0x1F, .a = 0x1F }  ,
  { .r = 0x00, .g = 0x00, .b = 0x1F, .a = 0x00 }  ,
  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x00 }  ,
  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x1F }  ,
  { .r = 0x00, .g = 0x1F, .b = 0x00, .a = 0x00 }  ,
  { .r = 0x00, .g = 0x1F, .b = 0x00, .a = 0x1F }  ,
  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x00 }  ,
  { .r = 0x1F, .g = 0x00, .b = 0x00, .a = 0x1F },
};

static struct color current_colors[LED_COUNT];

static int led_set;
static int note_states[256] = { -1 };
static int note_for_led[LED_COUNT];

static size_t tap_tempo_start;
static size_t tap_tempo_delta;
static size_t tap_tempo_next;

void led_setup() {
  int ret;
  setup_handlers();
  if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    exit(EXIT_FAILURE);
  }
  wiringPiSetup();
  pinMode(28, OUTPUT);
  pinMode(29, OUTPUT);

  struct color start_color = {
    .r = 0x1f, .g = 0x00, .b = 0x00, .a = 0x00,
  };
  struct color end_color = {
    .r = 0x00, .g = 0x00, .b = 0x1f, .a = 0x00,
  };

  for (int i = 0; i < 12; ++i) {
    colors[i] = lerp_color(start_color, end_color, i * 255 / 11);
  }

  for (int i = 0; i < 256; ++i) {
    note_states[i] = -1;
  }
  for (int i = 0; i < LED_COUNT; ++i) {
    note_for_led[i] = -1;
  }
  tap_tempo_next = 100000;
}

static int current_high_side = 0;
tick_count = 0;
void toggle_relays() {
  digitalWrite(29, current_high_side ? HIGH : LOW);
  digitalWrite(28, current_high_side ? LOW : HIGH);
  current_high_side = !current_high_side;
}

void led_tick_leds(struct msg * new_message, size_t tick) {
  int ret;

  if (tick == tap_tempo_next) {
    tap_tempo_next += tap_tempo_delta;
    printf("toggling relays, will do so again at %lu\n", tap_tempo_next);
    toggle_relays();

    struct color start_color = random_color();
    struct color end_color = random_color();

    for (int i = 0; i < 12; ++i) {
      colors[i] = lerp_color(start_color, end_color, i * 255 / 11);
    }
  }

  switch (new_message->tag) {
    case MSG_TAG_NOTE_ON:
      {
        if (new_message->data_msg.b_msg.note == 99) {
          printf("Starting tap tempo timer\n");
          tap_tempo_start = tick;
          break;
        }
        // led_set = (led_set + 1) % LED_COUNT; // random() % LED_COUNT;
        led_set = random() % LED_COUNT;
        int offset = 0;
        int note = new_message->data_msg.b_msg.note;
        for (offset = 0; note_for_led[(offset + led_set) % LED_COUNT] != -1 && offset < LED_COUNT; ++offset) {}
        if (offset == LED_COUNT) {
          printf("Out of LEDS %d\n", note_for_led[led_set]);
          break;
        }
        led_set = (led_set + offset) % LED_COUNT;
        if (note_states[note] != -1) {
          // Clear out the previous note in this slot
          note_for_led[note_states[note]] = -1;
        }
        note_for_led[led_set] = note;
        note_states[note] = led_set;
        current_colors[led_set] = colors[note % 12];
      }
      break;
    case MSG_TAG_NOTE_OFF:
      {
        if (new_message->data_msg.b_msg.note == 99) {
          toggle_relays();
          tap_tempo_delta = tick - tap_tempo_start;
          tap_tempo_next = tick + tap_tempo_delta;
          printf("Stopping tap tempo timer: %lu\n", tap_tempo_delta);
          break;
        }
        int note = new_message->data_msg.b_msg.note;
        if (note_states[note] == -1) {
          // Nothing to do, the note wasn't down apparently
          break;
        }
        note_for_led[note_states[note]] = -1;
        note_states[note] = -1;
      }
      break;
    case MSG_TAG_NO_MSG:
      {
        struct color target = {
          .r = 0x00, .g = 0x00, .b = 0x00, .a = 0x00
        };
        for (int i = 0; i < LED_COUNT; ++i) {
          if (note_for_led[i] != -1) {
            continue;
          }
          current_colors[i] = lerp_color(current_colors[i], target, 1);
        }
      }
      break;
  }

  for (int i = 0; i < LED_COUNT; ++i) {
    ledstring.channel[0].leds[i] = intcolor(current_colors[i]);
    // printf("Sending color: %08x at %d\n", ledstring.channel[0].leds[i], i);
//    ledstring.channel[0].leds[i] = 0xFFFFFFFF;
  }

  if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
    running = 0;
  }
}

void led_shutdown() {
  int ret;
  for (int i = 0; i < LED_COUNT; ++i) {
      ledstring.channel[0].leds[i] = 0;
  }

  if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
  }

  ws2811_fini(&ledstring);
  printf ("goodbye\n");
}
