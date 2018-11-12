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


#define ARRAY_SIZE(stuff)       (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                12
#define DMA                     10
//#define STRIP_TYPE            WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE              WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE            SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define LED_COUNT               16
#define LEDS_PER_UNIT            8

static uint8_t running = 1;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
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

int main(int argc, const char ** argv) {
  int ret;
  setup_handlers();
  if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    return ret;
  }


  struct color ls = { 0 }, le = { 0 };
  struct color rs = { 0 }, re = { 0 };
  int o = 0;
  int k = 0;
  int d = -1;
  le = random_color();
  re = random_color();
  printf("%08x %08x | %08x %08x\n", intcolor(ls), intcolor(le), intcolor(rs), intcolor(re));

  while (running) {
    if (o == 256) {
      ls = le;
      rs = re;
      le = random_color();
      re = random_color();
      k = (k + d + LEDS_PER_UNIT) % LEDS_PER_UNIT; // randint(0, LEDS_PER_UNIT);
      if (random() < RAND_MAX / 10) {
        d = randint(0, 2) * 2 - 1;
      }
      print_color(ls);
      print_color(le);
      print_color(rs);
      print_color(re);
      printf("%d\n-----\n", k);
      o = 0;
    }

    // printf("---\n");
    for (int i = 0; i < LEDS_PER_UNIT; ++i) {
      int f = abs(i - LEDS_PER_UNIT / 2);
      // printf("%d %d %d\n", i, f, f * (255 / (LEDS_PER_UNIT / 2)));
      struct color l = lerp_color(ls, le, (uint8_t)o);
      struct color r = lerp_color(rs, re, (uint8_t)o);
      struct color c = lerp_color(l, r, (uint8_t)(f * (255 / (LEDS_PER_UNIT / 2))));

      // c.r = c.g = c.b = 0;
      int a = o * o;
      switch( (i + LEDS_PER_UNIT - k) % LEDS_PER_UNIT) {
        case 0:
          if (d == 1) {
            c.a = (255 * 255 - a) / 256;
          } else {
            c.a = a / 256;
          }
          break;
        case 1:
          if (d == 1) {
            c.a = a / 256;
          } else {
            c.a = (255 * 255 - a) / 256;
          }
          break;
        default:
          c.a = 0;
      }
      c.a /= 2;
      // printf("%08x %08x %08x\n", intcolor(l), intcolor(r), intcolor(c));

      ledstring.channel[0].leds[i] = intcolor(c);
      ledstring.channel[0].leds[i + LEDS_PER_UNIT] = rotcolor(intcolor(c));
    }

    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
      fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
      break;
    }

    o += 1;

    // 60 frames /sec
    // usleep(1000000 / 60);
    usleep(1000);
  }

  for (int i = 0; i < LED_COUNT; ++i) {
      ledstring.channel[0].leds[i] = 0;
  }

  if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_render failed: %s\n", ws2811_get_return_t_str(ret));
  }

  ws2811_fini(&ledstring);
  printf ("goodbye\n");
  return ret;
}
