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

#ifndef __WLW_H
#define __WLW_H

typedef struct
{
  int fd;
} wlw_conn;

// wire types
typedef uint32_t wlw_word;
typedef wlw_word wlw_uint;
typedef struct
{
  wlw_word id;
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
  wlw_object object_id;
  wlw_word size_opcode;
} wlw_header;

wlw_conn wlw_open ();
#define WLW_MSG_SIZE 256
void wlw_send (wlw_conn self, wlw_header *hdr);
uint16_t wlw_recv (wlw_conn self, wlw_word out[WLW_MSG_SIZE]);

#endif // __WLW_H

#if defined(WLW_IMPLEMENTATION) && !defined(WLW_IMPLEMENTED)
#define WLW_IMPLEMENTED

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
wlw_send (wlw_conn self, wlw_header *hdr)
{
  uint16_t size = hdr->size_opcode >> 16;
  /*
  uint16_t len = size / sizeof (wlw_word);
  printf ("C->S (%d)", size);
  for (uint16_t i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *)hdr)[i]);
    }
  printf ("\n");
  */
  write (self.fd, hdr, size);
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

  /*
  uint16_t len = size / sizeof (wlw_word);
  printf ("S->C (%d)", size);
  for (uint16_t i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *)out)[i]);
    }
  printf ("\n");
  */

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

#endif // WLW_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#define WLW_IMPLEMENTATION
#include __FILE__

wlw_new_id
wlw_next_id ()
{
  // 1 is reserved for wl_display
  static wlw_word next = 2;
  return (wlw_new_id){ .repr = next++ };
}
// reserved
const wlw_object wl_display = { .id = 1 };
enum _wl_display_r
{
  _wl_display_r_sync,
  _wl_display_r_get_registry,
};
enum _wl_display_e
{
  _wl_display_e_global,
};
wlw_object
wl_display_get_registry (wlw_conn conn, wlw_object wl_display,
                         wlw_new_id new_id)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
    wlw_new_id new_id;
  } msg;
  msg.hdr.object_id = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_get_registry;
  msg.new_id = new_id;
  wlw_send (conn, &msg.hdr);
  return (wlw_object){ .id = new_id.repr };
}

wlw_object
wl_display_sync (wlw_conn conn, wlw_object wl_display, wlw_new_id new_id)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
  } msg;
  msg.hdr.object_id = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_sync;
  wlw_send (conn, &msg.hdr);
  return (wlw_object){ .id = new_id.repr };
}

enum _wl_registry_e
{
  _wl_registry_e_global,
};

void
wl_registry_global (wlw_object wl_registry, wlw_word *msg, wlw_uint **out_name,
                    wlw_string **out_interface, wlw_uint **out_version)
{
  uint8_t *msg_c = (uint8_t *)msg;
  wlw_header *hdr = (wlw_header *)msg;
  assert (hdr->object_id.id == wl_registry.id);
  uint16_t opcode = hdr->size_opcode & ((1 << 16) - 1);
  assert (opcode == _wl_registry_e_global);
  msg_c += sizeof (wlw_header);

  *out_name = (wlw_uint *)msg_c;
  msg_c += sizeof (wlw_uint);

  *out_interface = (wlw_string *)msg_c;
  wlw_word len = (*out_interface)->len;
  len += sizeof (len); // the field `len` itself
  len = (len + sizeof (len) - 1) & ~(sizeof (len) - 1);
  msg_c += len;

  *out_version = (wlw_uint *)msg_c;
  msg_c += sizeof (wlw_uint);
}

int
main ()
{
  wlw_conn conn = wlw_open ();
  printf ("hello wayland\n");

  wlw_object wl_registry
      = wl_display_get_registry (conn, wl_display, wlw_next_id ());

  (void)wl_registry;

  static wlw_word msg[256];
  for (;;)
    {
      wlw_recv (conn, msg);
      wlw_uint *name;
      wlw_string *interface;
      wlw_uint *version;
      wl_registry_global (wl_registry, msg, &name, &interface, &version);
      printf ("wl_registry:global(name=%d, interface=%s, version=%d)\n", *name,
              interface->str, *version);
    }
}
#endif
