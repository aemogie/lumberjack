#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef _WLW_H
#define _WLW_H

typedef uint32_t wlw_word;
typedef uint8_t wlw_byte;

// taxonomy note,
// "size" = in bytes
// "len" = count in array type, in this case wlw_word

// spec defines that size always fits top 16 bits of (wlw_header.size_opcode)
typedef uint16_t wlw_msg_size;
// only bottom 14 bits should ever be used, because,
// sizeof(wlw_word) == 4, 4 - 1 == 0b11
// two bits are unused
typedef uint16_t wlw_msg_len;

// division typically rounds down, this rounds up.
/*
"a"   :: the dividend
"+ b" :: bias it upwards by the divisor
"- 1" :: avoids doubly rounding up if "a" is already divisible by "b"
"/ b" :: proceed with round-down division as normal, on the biased dividend
*/
#define wlw_divide_up(a, b) (((a) + (b) - 1) / (b))

// takes a size in bytes and returns it as count of wlw_word, aligned up
// wlw_msg_size -> wlw_msg_len
#define wlw_size_to_len(size)                                                 \
  ((wlw_msg_len)wlw_divide_up ((wlw_msg_size)(size), sizeof (wlw_word)))
#define wlw_len_to_size(len)                                                  \
  ((wlw_msg_size)((wlw_msg_len)(len) * sizeof (wlw_word)))

#define WLW_MSG_MAX_LEN ((wlw_msg_len)256)
#define WLW_IO_BUFFER_SIZE ((wlw_msg_size)wlw_len_to_size (WLW_MSG_MAX_LEN))

// wire types
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

typedef struct
{
  wlw_header hdr;
  wlw_word payload[WLW_MSG_MAX_LEN - wlw_size_to_len (sizeof (wlw_header))];
} wlw_msg;
// because i dont trust myself to do math
_Static_assert (sizeof (wlw_msg) == wlw_len_to_size (WLW_MSG_MAX_LEN),
                "raw message struct is too large");

typedef struct
{
  // rw socket
  int fd;
  // reader state
  wlw_msg_size next_frame;
  wlw_msg_size read_end;
  wlw_byte buf[WLW_IO_BUFFER_SIZE];
} wlw_io_state;

void wlw_open (wlw_io_state *io);
void wlw_send (wlw_io_state *io, wlw_msg *msg);
wlw_msg_size wlw_recv (wlw_io_state *io, wlw_msg *msg);

// returned pointer is always same as dereferencing what was passed in
// the only utility of these functions is that they advance the head
wlw_uint *wlw_read_uint (wlw_word **head);
wlw_object *wlw_read_object (wlw_word **head);
wlw_new_id *wlw_read_new_id (wlw_word **head);
// dynamically sized, need to read from head to get size
wlw_string *wlw_read_string (wlw_word **head);

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
wlw_send (wlw_io_state *io, wlw_msg *msg)
{
  wlw_msg_size size = msg->hdr.size_opcode >> 16;

#if 0
  wlw_msg_len len = wlw_size_to_len (size);
  printf ("C->S (%d)", size);
  for (wlw_msg_len i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *)msg)[i]);
    }
  printf ("\n");
#endif

  wlw_byte *end = &((wlw_byte *)msg)[size];
  do
    {
      size -= write (io->fd, end - size, size);
    }
  while (__builtin_expect (size, 0));
}

wlw_msg_size
wlw_recv (wlw_io_state *io, wlw_msg *msg)
{

  wlw_msg_size remainder;
retry:
  remainder = io->read_end - io->next_frame;

  if (remainder < sizeof (wlw_header))
    goto refill_and_retry;

  wlw_msg_size size
      = (((wlw_header *)(&io->buf[io->next_frame]))->size_opcode) >> 16;
  assert (size < sizeof (io->buf));
  if (remainder < size)
    goto refill_and_retry;

#if 0
  int scale = 16;
  putchar ('[');
  for (int i = 0; i < WLW_IO_BUFFER_SIZE / scale; i++)
    {
      if (i < io->next_frame / scale)
        putchar (' ');
      else if (i < (io->next_frame + size) / scale)
        putchar ('#');
      else if (i < io->read_end / scale)
        putchar ('.');
      else
        putchar (' ');
    }
  putchar (']');
  putchar ('\n');
#endif

  memcpy (msg, &io->buf[io->next_frame], size);
  io->next_frame += size;

#if 0
  wlw_msg_len len = wlw_size_to_len (size);
  printf ("S->C (%d)", size);
  for (wlw_msg_len i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *)msg)[i]);
    }
  printf ("\n");
#endif

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

inline wlw_uint *
wlw_read_uint (wlw_word **head)
{
  return (wlw_uint *)(*head)++;
}

inline wlw_object *
wlw_read_object (wlw_word **head)
{
  return (wlw_object *)(*head)++;
}

inline wlw_new_id *
wlw_read_new_id (wlw_word **head)
{
  return (wlw_new_id *)(*head)++;
}

inline wlw_string *
wlw_read_string (wlw_word **head)
{
  wlw_string *ret = (wlw_string *)(*head);

  (*head)++; // skip the length field
  // align to wlw_word boundary
  *head += wlw_size_to_len (ret->len);
  return ret;
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
      _wlw_i_size,
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
_Static_assert (_wlw_i_size <= UINT8_MAX,
                "wlw_obj_map[i] cannot hold wlw_interface");

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
wlw_bookkeep_obj_setup_reserved (wlw_obj_map *obj_map,
                                 wlw_object *wlw_null_obj,
                                 wlw_object *wl_display)
{
  // assert wlw_state and in turn wlw_obj_map has been zero intialised
  assert (obj_map->free_tail == 0);
  // this underflows, but genid overflows it back to zero
  obj_map->free_tail--;
  *wlw_null_obj = wlw_bookkeep_obj_bind (obj_map, wlw_null_i,
                                         wlw_bookkeep_obj_genid (obj_map));
  *wl_display = wlw_bookkeep_obj_bind (obj_map, wlw_display_i,
                                       wlw_bookkeep_obj_genid (obj_map));
}

typedef struct
{
  wlw_msg msg;
  wlw_io_state io;
  wlw_obj_map obj_map;
} wlw_state;

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
  wlw_send (&wlw->io, (wlw_msg *)&msg);
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
  wlw_send (&wlw->io, (wlw_msg *)&msg.hdr);
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
  // redundant assert
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, wlw->msg.hdr.object)
          == wlw_registry_i);
  uint16_t opcode = wlw->msg.hdr.size_opcode & ((1 << 16) - 1);
  assert (opcode == _wl_registry_e_global);

  wlw_word *head = wlw->msg.payload;

  *out_name = wlw_read_uint (&head);
  *out_interface = wlw_read_string (&head);
  *out_version = wlw_read_uint (&head);
}

int
main ()
{
  wlw_state wlw = { 0 };
  wlw_open (&wlw.io);
  wlw_object wlw_null_obj, wl_display;
  wlw_bookkeep_obj_setup_reserved (&wlw.obj_map, &wlw_null_obj, &wl_display);

  printf ("listing all globals\n");
  wl_display_get_registry (&wlw, wl_display,
                           wlw_bookkeep_obj_genid (&wlw.obj_map));
  wl_display_sync (&wlw, wl_display, wlw_bookkeep_obj_genid (&wlw.obj_map));

  for (;;)
    {
      wlw_recv (&wlw.io, &wlw.msg);
      switch (wlw_bookkeep_obj_typeof (&wlw.obj_map, wlw.msg.hdr.object))
        {
        case wlw_registry_i:
          assert ((wlw.msg.hdr.size_opcode & ((1 << 16) - 1))
                  == _wl_registry_e_global);

          wlw_uint *name;
          wlw_string *interface;
          wlw_uint *version;
          wl_registry_global (&wlw, &name, &interface, &version);
          printf ("global(%d) = %s@%d\n", *name, interface->str, *version);
          break;
        case wlw_callback_i:
          wlw_bookkeep_obj_unbind (&wlw.obj_map, wlw.msg.hdr.object);
          printf ("done listing\n");
          exit (0);
          break;
        default:
          printf ("[!!!] %s:", wlw_interface_names[wlw_bookkeep_obj_typeof (
                                   &wlw.obj_map, wlw.msg.hdr.object)]);
          for (wlw_msg_len i = 0;
               i < wlw_size_to_len (wlw.msg.hdr.size_opcode >> 16); i++)
            {
              printf (" %08x", ((wlw_word *)&wlw.msg)[i]);
            }
          printf ("\n");
        };
    }
}

#define WLW_IMPLEMENTATION
#include "wlw.h"
#endif
