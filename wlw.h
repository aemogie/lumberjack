#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef __WLW_H
#define __WLW_H

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
  wlw_object object;
  wlw_word size_opcode;
} wlw_header;

#define WLW_MSG_MAX_LEN 256
#define WLW_IO_BUFFER_SIZE (WLW_MSG_MAX_LEN * sizeof (wlw_word))
// max 256 because thats the largest index for the free list
#define WLW_MAX_OBJECT_COUNT 32
typedef struct
{
  // rw socket
  int fd;

  // reader state
  uint16_t next_frame;
  uint16_t read_end;
  uint8_t io_buf[WLW_IO_BUFFER_SIZE];

  // object id
  uint8_t obj_tail; // just linear allocator for now, we can do free lists if
                    // we need it
  uint8_t obj[WLW_MAX_OBJECT_COUNT];
} wlw_conn;

void wlw_open (wlw_conn *conn);
void wlw_send (wlw_conn *conn, wlw_header *hdr);
uint16_t wlw_recv (wlw_conn *conn, wlw_word out[WLW_MSG_MAX_LEN]);

typedef enum
{
  wlw_null_i,    // not a real type
  wlw_display_i, // singleton reserved
  wlw_registry_i,
  wlw_callback_i,
} wlw_interface;

wlw_new_id wlw_make_id (wlw_conn *conn);

wlw_object wlw_bookkeep_obj_bind (wlw_conn *conn, wlw_interface interface,
                                  wlw_new_id new_id);
void wlw_bookkeep_obj_unbind (wlw_conn *conn, wlw_object object);
wlw_interface wlw_bookkeep_obj_typeof (wlw_conn *conn, wlw_object object);

#endif // __WLW_H

#if defined(WLW_IMPLEMENTATION) && !defined(WLW_IMPLEMENTED)
#define WLW_IMPLEMENTED

void
wlw_open (wlw_conn *conn)
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

  // cant do anything if it gets truncated, the api only allows
  // sizeof(addr.sun_path) anyway
  snprintf (addr.sun_path, sizeof (addr.sun_path), "%s/%s", rundir, sockname);

  int ret = connect (sockfd, (const struct sockaddr *)&addr, sizeof (addr));
  assert (ret == 0);

  // init struct

  // rw socket
  conn->fd = sockfd;

  // reader state
  conn->next_frame = 0;
  conn->read_end = 0;

  // obj id allocation
  conn->obj[0] = wlw_null_i;
  conn->obj[1] = wlw_display_i;
  conn->obj_tail = 1;
}

void
wlw_send (wlw_conn *self, wlw_header *hdr)
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
  write (self->fd, hdr, size);
}

uint16_t
wlw_recv (wlw_conn *conn, wlw_word out[WLW_MSG_MAX_LEN])
{

  uint16_t remainder;
retry:
  remainder = conn->read_end - conn->next_frame;
  if (remainder < sizeof (wlw_header))
    goto refill_and_retry;

  uint16_t size
      = (((wlw_header *)(&conn->io_buf[conn->next_frame]))->size_opcode) >> 16;
  assert (size < sizeof (conn->io_buf));
  if (remainder < size)
    goto refill_and_retry;

  memcpy (out, &conn->io_buf[conn->next_frame], size);
  conn->next_frame += size;

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

  // this should only ever hit atmost once per call. this should not ever loop
refill_and_retry:
  // compact
  memmove (&conn->io_buf[0], &conn->io_buf[conn->next_frame], remainder);
  conn->read_end = remainder;
  conn->next_frame = 0;
  // read
  int n = read (conn->fd, &conn->io_buf[conn->read_end],
                sizeof (conn->io_buf) - conn->read_end);
  assert (n > 0);
  conn->read_end += n;
  // retry
  goto retry;
}

inline wlw_new_id
wlw_make_id (wlw_conn *conn)
{
  wlw_new_id new_id = { .repr = ++conn->obj_tail };
  assert (new_id.repr < WLW_MAX_OBJECT_COUNT);
  return new_id;
}
inline wlw_object
wlw_bookkeep_obj_bind (wlw_conn *conn, wlw_interface interface,
                       wlw_new_id new_id)
{
  conn->obj[new_id.repr] = interface;
  return (wlw_object){ .id = new_id.repr };
}
inline void
wlw_bookkeep_obj_unbind (wlw_conn *conn, wlw_object object)
{
  if (conn->obj_tail == object.id)
    conn->obj_tail--;
  // else give up, unless we decide to make this into a freelist
}

inline wlw_interface
wlw_bookkeep_obj_typeof (wlw_conn *conn, wlw_object object)
{
  return conn->obj[object.id];
}

#endif // WLW_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#include "wlw.h"

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
wl_display_get_registry (wlw_conn *conn, wlw_object wl_display,
                         wlw_new_id registry)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
    wlw_new_id registry;
  } msg;
  msg.hdr.object = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_get_registry;
  msg.registry = registry;
  wlw_send (conn, &msg.hdr);
  return wlw_bookkeep_obj_bind (conn, wlw_registry_i, registry);
}

wlw_object
wl_display_sync (wlw_conn *conn, wlw_object wl_display, wlw_new_id callback)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
    wlw_new_id callback;
  } msg;
  msg.hdr.object = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_sync;
  msg.callback = callback;
  wlw_send (conn, &msg.hdr);
  return wlw_bookkeep_obj_bind (conn, wlw_callback_i, callback);
}

enum _wl_registry_e
{
  _wl_registry_e_global,
};

void
wl_registry_global (wlw_conn *conn, wlw_word *msg, wlw_uint **out_name,
                    wlw_string **out_interface, wlw_uint **out_version)
{
  uint8_t *msg_c = (uint8_t *)msg;
  wlw_header *hdr = (wlw_header *)msg;

  assert (wlw_bookkeep_obj_typeof (conn, hdr->object) == wlw_registry_i);
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
  wlw_conn conn = { 0 };
  wlw_open (&conn);

  wl_display_get_registry (&conn, wl_display, wlw_make_id (&conn));
  wl_display_sync (&conn, wl_display, wlw_make_id (&conn));

  wlw_word msg[256];
  for (;;)
    {
      wlw_recv (&conn, msg);
      wlw_header *hdr = (wlw_header *)msg;
      switch (wlw_bookkeep_obj_typeof (&conn, hdr->object))
        {
        case wlw_registry_i:
          assert ((hdr->size_opcode & ((1 << 16) - 1))
                  == _wl_registry_e_global);

          wlw_uint *name;
          wlw_string *interface;
          wlw_uint *version;
          wl_registry_global (&conn, msg, &name, &interface, &version);
          printf ("wl_registry:global(name=%d, interface=%s, version=%d)\n",
                  *name, interface->str, *version);
          break;
        case wlw_callback_i:
          exit (0);
        default:
          assert (0 && "unimplented");
        };
    }
}

#define WLW_IMPLEMENTATION
#include "wlw.h"
#endif
