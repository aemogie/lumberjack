#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define _HERE __HERE (__FILE__, __LINE__)
#define __HERE(f, l) ___HERE (f, l)
#define ___HERE(f, l) f ":" #l

#ifndef __WAYLAND_WIRE_H
#define __WAYLAND_WIRE_H

typedef struct
{
  int fd;
} wlw_conn;

typedef uint32_t wlw_word;
typedef wlw_word wlw_uint;
typedef struct
{
  wlw_word repr;
} wlw_object;

typedef struct
{
  wlw_word repr;
} wlw_new_id;

typedef struct
{
  wlw_word len;
  char str[];
} wlw_string;

typedef struct
{
  wlw_object object;
  wlw_word size_opcode;
} wlw_header;

// to be correct message "has" header, but as a base class a message
// only guarantees a header, ie its identical structurally
typedef wlw_header wlw_msg;

wlw_conn wlw_open ();
#define WLW_MSG_SIZE 256
void wlw_send (wlw_conn self, wlw_msg *msg);
uint16_t wlw_recv (wlw_conn self, wlw_word out[WLW_MSG_SIZE]);

#endif // __WAYLAND_WIRE_H

#if defined(WAYLAND_WIRE_IMPLEMENTATION) && !defined(WAYLAND_WIRE_IMPLEMENTED)
#define WAYLAND_WIRE_IMPLEMENTED

__attribute__ ((warn_unused_result)) wlw_conn
wlw_open ()
{
  int sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  assert (sockfd > 0);

  struct sockaddr_un addr = { 0 };
  addr.sun_family = AF_UNIX;

  const char *rundir = getenv ("XDG_RUNTIME_DIR");
  if (!rundir)
    rundir = "/run/user/1000";
  const char *sockname = getenv ("WAYLAND_DISPLAY");
  if (!sockname)
    sockname = "wayland-0";

  snprintf (addr.sun_path, sizeof (addr.sun_path), "%s/%s", rundir, sockname);

  int ret = connect (sockfd, (const struct sockaddr *)&addr, sizeof (addr));
  assert (ret == 0);

  return (wlw_conn){ .fd = sockfd };
}

void
wlw_send (wlw_conn self, wlw_msg *msg)
{
  int size = msg->size_opcode >> 16;
  write (self.fd, msg, size);
}

uint16_t
wlw_recv (wlw_conn self, wlw_word out[WLW_MSG_SIZE])
{
  static uint8_t buf[WLW_MSG_SIZE * sizeof (wlw_word)];
  static uint16_t next_frame = 0;
  static uint16_t read_end = 0;

  uint16_t remainder;
retry:
  remainder = read_end - next_frame;
  if (remainder < sizeof (wlw_header))
    goto refill_and_retry;

  uint16_t size = (((wlw_header *)(&buf[next_frame]))->size_opcode) >> 16;
  assert (size < sizeof (buf));
  if (remainder < size)
    goto refill_and_retry;

  memcpy (out, &buf[next_frame], size);
  next_frame += size;

  return size;

refill_and_retry:
  // compact
  memmove (&buf[0], &buf[next_frame], remainder);
  read_end = remainder;
  next_frame = 0;
  // read
  int n = read (self.fd, &buf[read_end], sizeof (buf) - read_end);
  assert (n > 0);
  read_end += n;
  // retry
  goto retry;
}

#endif // WAYLAND_WIRE_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#define WAYLAND_WIRE_IMPLEMENTATION
#include __FILE__

static const wlw_object wl_display = { .repr = 1 };
static const wlw_object wl_registry = { .repr = 2 };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-variable-sized-type-not-at-end"
typedef struct
{
  wlw_msg parent;
  wlw_new_id new_id;
} wl_display_get_registry;

typedef struct
{
  wlw_msg parent;
  wlw_uint name;
  wlw_string interface;
  wlw_uint __version;
} wl_registry_global;
#pragma GCC diagnostic pop

int
main ()
{
  wlw_conn conn = wlw_open ();
  printf ("hello wayland\n");

  wl_display_get_registry msg;
  msg.parent.object = wl_display;
  msg.parent.size_opcode = (sizeof (wl_display_get_registry)) << 16 | 1;
  msg.new_id = (wlw_new_id){ .repr = wl_registry.repr };
  wlw_send (conn, &msg.parent);

  static wlw_word out[256];
  for (;;)
    {
      wlw_recv (conn, out);
      wl_registry_global *g = (wl_registry_global *)out;
      printf ("%s@%d\n", g->interface.str, g->name);
    }
}
#endif
