/*----------------------------------------------------------------------------
wraith.c: simple Wraith Prism configuration utility

Copyright (c) 2020 Carl G. Ritson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
----------------------------------------------------------------------------*/

/****************************************************
 * Written using insights from:
 * - Adam Honse's OpenRGB:
 *   https://gitlab.com/CalcProgrammer1/OpenRGB/
 * - gfduszynski's cm-rgb:
 *   https://github.com/gfduszynski/cm-rgb
 *
 * To build (using hidapi and libusb):
 *   cc -O2 -Wall wraith.c -o wraith -lhidapi-libusb
 ****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <hidapi/hidapi.h>

#define AMD_WRAITH_PRISM_VID (0x2516)
#define AMD_WRAITH_PRISM_PID (0x0051)

#define CMD_SIZE (65)
#define REPLY_SIZE (64)

#define _TRUE (1)
#define _FALSE (0)

enum {
  MODE_STATIC = 0x01,
  MODE_COLOUR_CYCLE = 0x02,
  MODE_BREATH = 0x03,
  MODE_RING_RAINBOW = 0x05,
  MODE_RING_CHASE = 0xC3,
  MODE_RING_SWIRL = 0x4A,
  MODE_RING_DEFAULT = 0xFF
};

enum {
  CHANNEL_OFF = 0xFE,
  CHANNEL_LOGO = 0x05,
  CHANNEL_FAN = 0x06,
  CHANNEL_RING_STATIC = 0x00,
  CHANNEL_RING_BREATH = 0x01,
  CHANNEL_RING_COLOUR_CYCLE = 0x02,
  CHANNEL_RING_RAINBOW = 0x07,
  CHANNEL_RING_BOUNCE = 0x08,
  CHANNEL_RING_CHASE = 0x09,
  CHANNEL_RING_SWIRL = 0x0A,
  CHANNEL_RING_MORSE = 0x0B
};

enum {
  STATIC_SPEED = 0xFF
};

static const unsigned char breath_speed[] = { 0x3C, 0x37, 0x31, 0x2C, 0x26 };
static const unsigned char cycle_speed[] = { 0x96, 0x8C, 0x80, 0x6E, 0x68 };
static const unsigned char rainbow_speed[] = { 0x72, 0x68, 0x64, 0x62, 0x61 };
static const unsigned char chase_speed[] = { 0x77, 0x74, 0x6E, 0x6B, 0x67 };
static const unsigned char swirl_speed[] = { 0x77, 0x74, 0x6E, 0x6B, 0x67 };

static unsigned verbose_mode = _FALSE;

static void log_debug(const char *fmt, ...) {
  va_list args;

  if (verbose_mode) {
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
}

static void log_bytes(const char *header, unsigned char *buffer, unsigned length) {
  if (!verbose_mode)
    return;

  fprintf(stderr, "%s\n", header);
  for (unsigned i = 0; i < length; ++i) {
    fprintf(stderr, "%s0x%02x%s",
            ((i % 8) != 0) && (i > 0) ? ", " : "",
            buffer[i],
            (i > 0) && ((i + 1) % 8) == 0 ? "\n" : "");
  }
  fprintf(stderr, "\n");
}

static void log_error(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

static int wraith_communicate(hid_device *dev, unsigned char *buf) {
  int ret;
  log_bytes("Sending:", buf, CMD_SIZE);
  ret = hid_write(dev, buf, CMD_SIZE);
  if (ret < 0) {
    log_error("Device write failed.\n");
    return -1;
  }
  if (ret < CMD_SIZE) {
    log_error("Device write failed; not all command bytes sent.\n");
    return -1;
  }
  ret = hid_read(dev, buf, REPLY_SIZE);
  if (ret < 0) {
    log_error("Device read failed.\n");
    return -1;
  }
  log_bytes("Received:", buf, ret);
  if (ret != REPLY_SIZE) {
    log_error("Device read failed; short reply (only %d bytes)\n", ret);
    return -1;
  }
  return 0;
}

static void wraith_enable(hid_device *dev) {
  unsigned char buf[CMD_SIZE];
  memset(buf, 0, sizeof(buf));

  buf[1] = 0x41;
  buf[2] = 0x80;

  log_debug("Enable wraith controller.\n");
  wraith_communicate(dev, buf);
}

static void wraith_apply(hid_device *dev) {
  unsigned char buf[CMD_SIZE];
  memset(buf, 0, sizeof(buf));

  buf[1] = 0x51;
  buf[2] = 0x28;
  buf[5] = 0xE0;

  log_debug("Apply settings.\n");
  wraith_communicate(dev, buf);
}

static void hz_to_bytes(unsigned char *dst,
                        unsigned hz) {
  if (hz == 0) {
    dst[0] = 0x00;
    dst[1] = 0xFF;
    dst[2] = 0x4A;
  } else {
    /* FIXME: totally unconvinced this is correct */
    float v = 1500000.0f / ((float) hz);
    unsigned m = (unsigned) (v / 256.0);
    float r = v / (((float) m) + 0.75);
    unsigned r0 = (unsigned) r;
    unsigned r1 = (unsigned) ((r - (float)r0) * 256.0);
    dst[0] = m < 255 ? m : 255;
    dst[1] = r1;
    dst[2] = r0;
  }
}

static void wraith_mirage(hid_device *dev,
                          unsigned rhz,
                          unsigned ghz,
                          unsigned bhz) {
  unsigned char buf[CMD_SIZE];
  memset(buf, 0, sizeof(buf));

  buf[0x01] = 0x51;
  buf[0x02] = 0x71;

  buf[0x05] = 0x01;
  /* 0x06 0x07 0x08 */
  hz_to_bytes(&(buf[0x06]), 0);
  buf[0x09] = 0x02;
  /* 0x0a 0x0b 0x0c */
  hz_to_bytes(&(buf[0x0a]), rhz);
  buf[0x0d] = 0x03;
  /* 0x0e 0x0f 0x10 */
  hz_to_bytes(&(buf[0x0e]), ghz);
  buf[0x11] = 0x04;
  /* 0x12 0x13 0x14 */
  hz_to_bytes(&(buf[0x12]), bhz);

  log_debug("Programming mirage.\n");
  wraith_communicate(dev, buf);
}

static void wraith_effect_update(hid_device *dev,
                          unsigned char channel,
                          unsigned char speed,
                          unsigned char flags,
                          unsigned char mode,
                          unsigned char brightness,
                          unsigned char red1,
                          unsigned char green1,
                          unsigned char blue1,
                          unsigned char red2,
                          unsigned char green2,
                          unsigned char blue2) {
  unsigned char buf[CMD_SIZE];
  memset(buf, 0xFF, sizeof(buf));

  buf[0x00] = 0x00;
  buf[0x01] = 0x51;
  buf[0x02] = 0x2C;
  buf[0x03] = 0x01;
  buf[0x04] = 0x00;

  buf[0x05] = channel;
  buf[0x06] = speed;
  buf[0x07] = flags;
  buf[0x08] = mode;

  buf[0x09] = 0xFF; /* ?? */

  buf[0x0A] = brightness;

  buf[0x0B] = red1;
  buf[0x0C] = green1;
  buf[0x0D] = blue1;

  buf[0x0E] = red2;
  buf[0x0F] = green2;
  buf[0x10] = blue2;

  log_debug("Programming channel 0x%02x.\n", channel);
  wraith_communicate(dev, buf);
}

static void wraith_query_channel(hid_device *dev, unsigned char channel) {
  unsigned verbose = verbose_mode;
  unsigned char buf[CMD_SIZE];
  memset(buf, 0, sizeof(buf));

  buf[0x01] = 0x40;
  buf[0x02] = 0x21;
  buf[0x03] = channel;

  verbose_mode = _TRUE;
  log_debug("Reading channel 0x%02x:\n", channel);
  wraith_communicate(dev, buf);
  verbose_mode = verbose;
}

static void wraith_channel_map(hid_device *dev,
                          unsigned char *ring,
                          unsigned char logo,
                          unsigned char fan) {
  unsigned char buf[CMD_SIZE];
  memset(buf, 0, sizeof(buf));

  buf[0x01] = 0x51;
  buf[0x02] = 0xA0;
  buf[0x03] = 0x01;
  buf[0x06] = 0x03;
  buf[0x09] = logo;
  buf[0x0A] = fan;

  for (int i = 0; i < 15; ++i)
    buf[0x0B + i] = ring[i];

  log_debug("Programming channel map.\n");
  wraith_communicate(dev, buf);
}

static hid_device *wraith_open(void) {
  struct hid_device_info *devs = hid_enumerate(AMD_WRAITH_PRISM_VID, AMD_WRAITH_PRISM_PID);
  struct hid_device_info *info = devs;
  while (info) {
    if ((info->vendor_id == AMD_WRAITH_PRISM_VID) &&
        (info->product_id == AMD_WRAITH_PRISM_PID) &&
        (info->interface_number == 1)) {
      log_debug("Wraith device found.\n");
      return hid_open_path(info->path);
    }
    info = info->next;
  }
  log_debug("No wraith devices found.\n");
  return NULL;
}

static void help(const char *name) {
  fprintf(stdout,
          "%s [-v] <command> [<command> ...]\n"
          "  a sequence of commands from the following:\n"
          "    - ring-map <channel> [<channel>] ...\n"
          "        set channel map in order: ring-led1 ... ring-led15\n"
          "        missing channels will be set to last sequence value\n"
          "        <channel> can one of:\n"
          "           static, cycle, breath, rainbow, bounce, swirl, bounce, chase, morse, off\n"
          "    - effect [logo|fan] <mode> <speed> <brightness> <red1> <blue1> <green1>\n"
          "                                                   [<red2> <blue2> <green2> <flags>]\n"
          "        set effect for logo or fan\n"
          "        <mode> can be static, cycle or breath\n"
          "        <speed> is a value 1 to 5 (ignore for static)\n"
          "        <flags> is a hex value it sets a not fully documented byte\n"
          "                0x80 = random colour\n"
          "                0x40 = blend colours (at least for breath mode)\n"
          "                0x20 = fixed colour\n"
          "                0x01 = reverse order\n"
          "    - ring-effect <channel> <speed> <brightness> <red1> <blue1> <green1>\n"
          "                                                [<red2> <blue2> <green2> <flags>]\n"
          "        configure effect for ring channels\n"
          "        <channel> can one of:\n"
          "           static, cycle, breath, rainbow, bounce, swirl, bounce, chase, morse, off\n"
          "        <speed> and <flags are the same as effect command\n"
          "    - mirage <red-hz> <green-hz> <blue-hz>\n"
          "        program mirage rates, set to 0 to disable\n"
          "    - query-channel <id>\n"
          "        print out raw state of channel\n",
          name);
}

#define FIND_VALUE(str, strings, values) \
  if (str == NULL) \
    return -1; \
  for (int i = 0; i < sizeof(strings); ++i) \
    if (strcmp(strings[i], str) == 0) \
      return values[i]; \
  log_error("Unable to parse \"%s\".\n", str); \
  return -1;

static int parse_channel(const char *str) {
  const char *strings[] = { "logo", "fan" };
  unsigned char values[] = { CHANNEL_LOGO, CHANNEL_FAN };
  FIND_VALUE(str, strings, values);
}

static int parse_mode(const char *str) {
  const char *strings[] = { "static", "cycle", "breath" };
  unsigned char values[] = { MODE_STATIC, MODE_COLOUR_CYCLE, MODE_BREATH };
  FIND_VALUE(str, strings, values);
}

static int parse_ring_channel(const char *str) {
  const char *strings[] = {
    "static", "cycle", "breath", "rainbow",
    "bounce", "chase", "swirl", "morse",
    "off"
  };
  unsigned char values[] = {
    CHANNEL_RING_STATIC,
    CHANNEL_RING_COLOUR_CYCLE,
    CHANNEL_RING_BREATH,
    CHANNEL_RING_RAINBOW,
    CHANNEL_RING_BOUNCE,
    CHANNEL_RING_CHASE,
    CHANNEL_RING_SWIRL,
    CHANNEL_RING_MORSE,
    CHANNEL_OFF
  };
  FIND_VALUE(str, strings, values);
}

static int parse_value(const char *str, int min, int max, int ignore_error, int defv) {
  int value;

  if (str == NULL)
    return defv;
  value = strtoul(str, (char **)NULL, 0);
  if (value >= min && value <= max)
    return value;

  if (!ignore_error)
    log_error("Unable to parse value string \"%s\" (range %d to %d).\n", str, min, max);
  return defv;
}

static int parse_ring_map(hid_device *dev, char *buffer) {
  unsigned char ring[15];
  int i;
  for (i = 0; i < 15; ++i) {
    char *channel = strsep(&buffer, " ");
    if (!channel)
      break;
    int value = parse_ring_channel(channel);
    if (value < 0)
      return -1;
    ring[i] = (unsigned char) value;
  }
  if (i == 0)
    return -1;
  for (char value = ring[i - 1]; i < 15; ++i)
    ring[i] = value;
  wraith_channel_map(dev, ring, CHANNEL_LOGO, CHANNEL_FAN);
  return 1;
}

static int parse_effect(hid_device *dev, char *buffer) {
  int channel = parse_channel(strsep(&buffer, " "));
  int mode = parse_mode(strsep(&buffer, " "));
  int speed = parse_value(strsep(&buffer, " "), 1, 5, _FALSE, -1);
  int brightness = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int red = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int green = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int blue = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int red2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int green2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int blue2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int flags = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0x20);
  if (channel >= 0 && mode >= 0 && speed >= 0 &&
      brightness >= 0 && red >= 0 && blue >= 0 && green >= 0) {
    switch (mode) {
      case MODE_STATIC:
        speed = STATIC_SPEED;
        break;
      case MODE_COLOUR_CYCLE:
        speed = cycle_speed[speed - 1];
        break;
      case MODE_BREATH:
        speed = breath_speed[speed - 1];
        break;
      default:
        return -1;
    }
    wraith_effect_update(dev,
        channel, speed,
        flags, mode,
        brightness, red, green, blue,
        red2, green2, blue2
    );
    return 1;
  }
  return -1;
}

static int parse_ring_effect(hid_device *dev, char *buffer) {
  int channel = parse_ring_channel(strsep(&buffer, " "));
  int speed = parse_value(strsep(&buffer, " "), 1, 5, _FALSE, -1);
  int brightness = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int red = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int green = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int blue = parse_value(strsep(&buffer, " "), 0, 255, _FALSE, -1);
  int red2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int green2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int blue2 = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0);
  int flags = parse_value(strsep(&buffer, " "), 0, 255, _TRUE, 0x20);
  if (channel >= 0 && speed >= 0 && brightness >= 0 &&
      red >= 0 && blue >= 0 && green >= 0) {
    int mode = MODE_RING_DEFAULT;
    switch (channel) {
      case CHANNEL_RING_STATIC:
        speed = STATIC_SPEED;
        break;
      case CHANNEL_RING_COLOUR_CYCLE:
        speed = cycle_speed[speed - 1];
        break;
      case CHANNEL_RING_BREATH:
        mode = MODE_BREATH;
        speed = breath_speed[speed - 1];
        break;
      case CHANNEL_RING_RAINBOW:
        mode = MODE_RING_RAINBOW;
        speed = rainbow_speed[speed - 1];
        break;
      case CHANNEL_RING_BOUNCE:
        speed = 0;
        break;
      case CHANNEL_RING_CHASE:
        mode = MODE_RING_CHASE;
        speed = chase_speed[speed - 1];
        break;
      case CHANNEL_RING_SWIRL:
        mode = MODE_RING_SWIRL;
        speed = swirl_speed[speed - 1];
        break;
      case CHANNEL_RING_MORSE:
        mode = MODE_RING_RAINBOW;
        speed = 0;
        break;
      default:
        return -1;
    }
    wraith_effect_update(dev,
        channel, speed,
        flags, mode,
        brightness, red, green, blue,
        red2, green2, blue2
    );
    return 1;
  }
  return -1;
}

static int parse_mirage(hid_device *dev, char *buffer) {
  int red = parse_value(strsep(&buffer, " "), 0, 65536, _FALSE, -1);
  int green = parse_value(strsep(&buffer, " "), 0, 65336, _FALSE, -1);
  int blue = parse_value(strsep(&buffer, " "), 0, 65536, _FALSE, -1);
  if (red >= 0 && green >= 0 && blue >= 0) {
    wraith_mirage(dev, red, green, blue);
    return 1;
  }
  return -1;
}

static int parse_query_channel(hid_device *dev, char *buffer) {
  int id = parse_value(strsep(&buffer, " "), 0, 0x0f, _FALSE, -1);
  if (id >= 0) {
    wraith_query_channel(dev, id);
    return 0;
  }
  return -1;
}

static int parse_command(hid_device *dev, const char *command) {
  char _buffer[256];
  char *cmd, *buffer = _buffer;

  log_debug("Parsing command: \"%s\"\n", command);

  if (strlen(command) >= sizeof(_buffer))
    return -1;
  strncpy(buffer, command, sizeof(_buffer));

  cmd = strsep(&buffer, " ");
  if (strcmp(cmd, "ring-map") == 0) {
    return parse_ring_map(dev, buffer);
  } else if (strcmp(cmd, "effect") == 0) {
    return parse_effect(dev, buffer);
  } else if (strcmp(cmd, "ring-effect") == 0) {
    return parse_ring_effect(dev, buffer);
  } else if (strcmp(cmd, "mirage") == 0) {
    return parse_mirage(dev, buffer);
  } else if (strcmp(cmd, "query-channel") == 0) {
    return parse_query_channel(dev, buffer);
  }

  return -1;
}

int main(int argc, char *argv[]) {
  hid_device *dev;
  int argc_offset = 1;
  int ret = 0;
  int res;

  /* check for verbose or help flags */
  if (argc >= 2) {
    if (strcmp(argv[1], "-v") == 0) {
      verbose_mode = _TRUE;
      argc_offset += 1;
    } else if (argv[1][0] == '-') {
      help(argv[0]);
      return 1;
    }
  }
  if ((argc - argc_offset) <= 0) {
    help(argv[0]);
    return 1;
  }

  /* initialise hid library */
  res = hid_init();
  if (res < 0) {
    log_error("Unable to init libhid\n");
    return 1;
  }

  /* open device and execute commands */
  dev = wraith_open();
  if (dev) {
    unsigned n_commands = 0;
    /* send device enable */
    wraith_enable(dev);

    /* run commands */
    for (int i = argc_offset; i < argc; ++i) {
      const char *command = argv[i];
      int ret = parse_command(dev, command);
      if (ret < 0) {
        log_error("Unable to parse command \"%s\"\n", command);
        ret = 1;
        break;
      } else {
        n_commands += ret;
      }
    }

    /* send apply if any commands were executed */
    if (n_commands > 0)
      wraith_apply(dev);

    /* tidy up */
    hid_close(dev);
  } else {
    log_error("No device found or device could not be opened.\n");
    ret = 1;
  }

  return ret;
}
