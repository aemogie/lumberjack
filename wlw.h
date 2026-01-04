#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _WLW_H
#define _WLW_H

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

typedef struct
{
  wlw_header hdr;
  wlw_word
      payload[WLW_MSG_MAX_LEN - (sizeof (wlw_header) / sizeof (wlw_word))];
} wlw_raw_msg;
// because i dont trust myself to do math
_Static_assert (sizeof (wlw_raw_msg) <= (WLW_MSG_MAX_LEN * sizeof (wlw_word)),
                "raw message struct is too large");

#define WLW_IO_BUFFER_SIZE (WLW_MSG_MAX_LEN * sizeof (wlw_word))
typedef struct
{
  // rw socket
  int fd;
  // reader state
  uint16_t next_frame;
  uint16_t read_end;
  uint8_t buf[WLW_IO_BUFFER_SIZE];
} wlw_io_state;

void wlw_open (wlw_io_state *io);
void wlw_send (wlw_io_state *io, wlw_raw_msg *msg);
uint16_t wlw_recv (wlw_io_state *io, wlw_raw_msg *msg);

#define _WLW_SIZED_WIRE_TYPES(X)                                              \
  X (header)                                                                  \
  X (object)                                                                  \
  X (uint)                                                                    \
  X (new_id)

// returns len in number of wlw_words
// not bytes because if a field is not aligned thats a bug
#define X(type) uint16_t wlw_read_##type (wlw_word *head, wlw_##type **out);
_WLW_SIZED_WIRE_TYPES (X)
#undef X

// dynamically sized, need to read from head to get size
uint16_t wlw_read_string (wlw_word *head, wlw_string **out);

#endif // _WLW_H

#if defined(WLW_IMPLEMENTATION) && !defined(WLW_IMPLEMENTED)
#define WLW_IMPLEMENTED

void
wlw_open (wlw_io_state *io)
{
  int sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  assert (sockfd >= 0);

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
  io->fd = sockfd;

  // reader state
  io->next_frame = 0;
  io->read_end = 0;
}

void
wlw_send (wlw_io_state *io, wlw_raw_msg *msg)
{
  uint16_t size = msg->hdr.size_opcode >> 16;
  /*
  uint16_t len = size / sizeof (wlw_word);
  printf ("C->S (%d)", size);
  for (uint16_t i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *)msg)[i]);
    }
  printf ("\n");
  */
  uint8_t *end = &((uint8_t *)msg)[size];

  do
    {
      size -= write (io->fd, end - size, size);
    }
  while (__builtin_expect (size, 0));
}

uint16_t
wlw_recv (wlw_io_state *io, wlw_raw_msg *msg)
{

  uint16_t remainder;
retry:
  remainder = io->read_end - io->next_frame;
  if (remainder < sizeof (wlw_header))
    goto refill_and_retry;

  uint16_t size
      = (((wlw_header *)(&io->buf[io->next_frame]))->size_opcode) >> 16;
  assert (size < sizeof (io->buf));
  if (remainder < size)
    goto refill_and_retry;

  memcpy (msg, &io->buf[io->next_frame], size);
  io->next_frame += size;

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
  memmove (&io->buf[0], &io->buf[io->next_frame], remainder);
  io->read_end = remainder;
  io->next_frame = 0;
  // read
  int n
      = read (io->fd, &io->buf[io->read_end], sizeof (io->buf) - io->read_end);
  assert (n > 0);
  io->read_end += n;
  // retry
  goto retry;
}

#define X(type)                                                               \
  inline uint16_t wlw_read_##type (wlw_word *head, wlw_##type **out)          \
  {                                                                           \
    *out = (wlw_##type *)head;                                                \
    return sizeof (wlw_##type) / sizeof (wlw_word);                           \
  }

_WLW_SIZED_WIRE_TYPES (X)
#undef X

inline uint16_t
wlw_read_string (wlw_word *head, wlw_string **out)
{
  uint16_t len = 0;
  *out = (wlw_string *)head;
  wlw_word str_len = (*out)->len;
  len++; // skip the length field
  // align to wlw_word boundary
  len += (str_len + sizeof (wlw_word) - 1) / sizeof (wlw_word);
  return len;
}
#endif // WLW_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#include "wlw.h"

#define WLW_INTERFACES(X)                                                     \
  X (null)                                                                    \
  X (display)                                                                 \
  X (registry)                                                                \
  X (callback)

typedef enum
{
#define X(name) wlw_##name##_i,
  WLW_INTERFACES (X)
#undef X
} wlw_interface;

const char *wlw_interface_names[] = {
#define X(name) "wl_" #name,
  WLW_INTERFACES (X)
#undef X
};

#define WLW_MAX_OBJECT_COUNT 32
typedef struct
{
  // just linear allocator for now, we can do free lists if we need it
  uint32_t free_tail;
  uint8_t items[WLW_MAX_OBJECT_COUNT];
} wlw_obj_map;

wlw_new_id
wlw_bookkeep_obj_genid (wlw_obj_map *obj_map)
{
  wlw_new_id new_id = { .repr = ++obj_map->free_tail };
  assert (new_id.repr < WLW_MAX_OBJECT_COUNT);
  return new_id;
}
wlw_object
wlw_bookkeep_obj_bind (wlw_obj_map *obj_map, wlw_interface interface,
                       wlw_new_id new_id)
{
  // printf ("binding id %d to %s\n", new_id.repr,
  //         wlw_interface_names[interface]);
  obj_map->items[new_id.repr] = interface;
  return (wlw_object){ .id = new_id.repr };
}
void
wlw_bookkeep_obj_unbind (wlw_obj_map *obj_map, wlw_object object)
{
  if (obj_map->free_tail == object.id)
    obj_map->free_tail--;
  // else give up, unless we decide to make this into a freelist
}

wlw_interface
wlw_bookkeep_obj_typeof (wlw_obj_map *obj_map, wlw_object object)
{
  return obj_map->items[object.id];
}

void
wlw_bookkeep_obj_setup_reserved (wlw_obj_map *obj_map)
{
  assert (obj_map->free_tail == 0);
  obj_map->free_tail--; // underflows i assume but thats fine
  wlw_bookkeep_obj_bind (obj_map, wlw_null_i,
                         wlw_bookkeep_obj_genid (obj_map));
  wlw_bookkeep_obj_bind (obj_map, wlw_display_i,
                         wlw_bookkeep_obj_genid (obj_map));
}

typedef struct
{
  uint16_t parse_head; // index into raw.payload
  wlw_raw_msg raw;
} wlw_msg;

typedef struct
{
  wlw_msg msg;
  wlw_io_state io;
  wlw_obj_map obj_map;
} wlw_state;

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
wl_display_get_registry (wlw_state *wlw, wlw_object wl_display,
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
  wlw_send (&wlw->io, (wlw_raw_msg *)&msg);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, wlw_registry_i, registry);
}

wlw_object
wl_display_sync (wlw_state *wlw, wlw_object wl_display, wlw_new_id callback)
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
  wlw_send (&wlw->io, (wlw_raw_msg *)&msg.hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, wlw_callback_i, callback);
}

enum _wl_registry_e
{
  _wl_registry_e_global,
};

void
wl_registry_global (wlw_state *wlw, wlw_uint **out_name,
                    wlw_string **out_interface, wlw_uint **out_version)
{
  wlw->msg.parse_head = 0;
  // redundant assert
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, wlw->msg.raw.hdr.object)
          == wlw_registry_i);
  uint16_t opcode = wlw->msg.raw.hdr.size_opcode & ((1 << 16) - 1);
  assert (opcode == _wl_registry_e_global);

#define wlw_parse(out_var, type)                                              \
  wlw->msg.parse_head += wlw_read_##type (                                    \
      &wlw->msg.raw.payload[wlw->msg.parse_head], out_var);

  wlw_parse (out_name, uint);
  wlw_parse (out_interface, string);
  wlw_parse (out_version, uint);
#undef wlw_parse
}

int
main ()
{
  wlw_state wlw = { 0 };
  wlw_open (&wlw.io);
  wlw_bookkeep_obj_setup_reserved (&wlw.obj_map);

  printf ("listing all globals\n");
  wl_display_get_registry (&wlw, wl_display,
                           wlw_bookkeep_obj_genid (&wlw.obj_map));
  wl_display_sync (&wlw, wl_display, wlw_bookkeep_obj_genid (&wlw.obj_map));

  for (;;)
    {
      wlw_recv (&wlw.io, &wlw.msg.raw);
      switch (wlw_bookkeep_obj_typeof (&wlw.obj_map, wlw.msg.raw.hdr.object))
        {
        case wlw_registry_i:
          assert ((wlw.msg.raw.hdr.size_opcode & ((1 << 16) - 1))
                  == _wl_registry_e_global);

          wlw_uint *name;
          wlw_string *interface;
          wlw_uint *version;
          wl_registry_global (&wlw, &name, &interface, &version);
          printf ("wl_registry:global(name=%d, interface=%s, version=%d)\n",
                  *name, interface->str, *version);
          break;
        case wlw_callback_i:
          wlw_bookkeep_obj_unbind (&wlw.obj_map, wlw.msg.raw.hdr.object);
          printf ("done listing\n");
          exit (0);
        default:
          assert (0 && "unimplemented");
        };
    }
}

#define WLW_IMPLEMENTATION
#include "wlw.h"
#endif
