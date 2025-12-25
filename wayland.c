#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define _CONCAT2(a, b) __CONCAT2 (a, b)
#define __CONCAT2(a, b) a##b
#define _HERE __HERE (__FILE__, __LINE__)
#define __HERE(f, l) ___HERE (f, l)
#define ___HERE(f, l) f ":" #l

#ifndef __WAYLAND_WIRE_H
#define __WAYLAND_WIRE_H
#ifndef WAYLAND_WIRE_MODULE_NAME
#define WAYLAND_WIRE_MODULE_NAME wlw_
#endif
#define wlw(name) _CONCAT2 (WAYLAND_WIRE_MODULE_NAME, name)

typedef struct
{
  int fd;
} wlw (conn);

typedef uint32_t wlw (word);
typedef struct
{
  wlw (word) repr;
} wlw (object);

typedef struct
{
  wlw (object) object;
  wlw (word) size_opcode;
} wlw (msg);

static const wlw (object) wl_display = { .repr = 1 };
static const wlw (object) wl_registry = { .repr = 2 };

typedef struct
{
  wlw (msg) parent;
  wlw (object) new_id;
} wl_display_get_registry;

wlw (conn) wlw (open) ();

void wlw (send) (wlw (conn) self, wlw (msg) * msg);
#endif // __WAYLAND_WIRE_H

#if defined(WAYLAND_WIRE_IMPLEMENTATION) && !defined(WAYLAND_WIRE_IMPLEMENTED)
#define WAYLAND_WIRE_IMPLEMENTED

__attribute__ ((warn_unused_result))
wlw (conn) wlw (open) ()
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

  return ((wlw (conn)){ .fd = sockfd });
}

void
wlw (send) (wlw (conn) self, wlw (msg) * msg)
{
  uint16_t size = msg->size_opcode >> 16;
  printf ("C->S ");
  for (int i = 0; i < size; i++)
    {
      if (i && i % 4 == 0)
        printf (" ");
      printf ("%02x", ((uint8_t *)msg)[i]);
    }
  printf ("\n");
  write (self.fd, msg, size);
  perror (_HERE);
  // i think i need to impl wlw(recv) to check if this actually did anything
}

#endif // WAYLAND_WIRE_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#define WAYLAND_WIRE_IMPLEMENTATION
#include __FILE__

int
main ()
{
  printf ("hello wayland\n");
  wlw_conn conn = wlw_open ();
  wl_display_get_registry msg = { 0 };
  msg.parent.object = wl_display;
  msg.parent.size_opcode = (sizeof (wl_display_get_registry)) << 16 | 1;
  msg.new_id = wl_registry;
  wlw_send (conn, &msg.parent);
}
#endif
