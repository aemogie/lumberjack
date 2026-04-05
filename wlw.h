#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>
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

#define WLW_IO_BUFFER_SIZE (wlw_len_to_size (256))

// wire types
typedef wlw_word wlw_uint;
typedef struct
{
  wlw_word id;
} wlw_object;

typedef struct
{
  wlw_word repr;
} wlw_static_new_id;

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
  wlw_word payload[];
} wlw_msg_view;

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
void wlw_send (wlw_io_state *io, wlw_msg_view *msg);
wlw_msg_view *wlw_recv (wlw_io_state *io);

// returned pointer is always same as dereferencing what was passed in
// the only utility of these functions is that they advance the head
static inline wlw_uint *wlw_read_uint (wlw_word **head);
static inline wlw_object *wlw_read_object (wlw_word **head);
static inline wlw_static_new_id *wlw_read_static_new_id (wlw_word **head);
// dynamically sized, need to read from head to get size
static inline wlw_string *wlw_read_string (wlw_word **head);

static inline void wlw_write_uint (wlw_word **head, wlw_uint *val);
static inline void wlw_write_object (wlw_word **head, wlw_object *obj);
static inline void wlw_write_static_new_id (wlw_word **head,
                                            wlw_static_new_id *id);
// dynamically sized, need to read from head to get size
static inline void wlw_write_string (wlw_word **head, wlw_string *str);
static inline void wlw_write_dynamic_new_id (wlw_word **head,
                                             wlw_string *interface,
                                             wlw_uint *version,
                                             wlw_static_new_id *id);

#endif // _WLW_H

#if defined(WLW_IMPLEMENTATION) && !defined(WLW_IMPLEMENTED)
#define WLW_IMPLEMENTED

void
wlw_open (wlw_io_state *io)
{
  int sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  assert (sockfd >= 0);

  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;

  const char *rundir = getenv ("XDG_RUNTIME_DIR");
  if (!rundir)
    rundir = "/run/user/1000";
  size_t rundir_size = strlen (rundir);
  const char *sockname = getenv ("WAYLAND_DISPLAY");
  if (!sockname)
    sockname = "wayland-0";
  size_t sockname_size = strlen (sockname);

  assert (rundir_size + sizeof (char) + sockname_size + sizeof (char)
          <= sizeof (addr.sun_path));
  memcpy (&addr.sun_path[0], rundir, rundir_size);
  addr.sun_path[rundir_size] = '/';
  memcpy (&addr.sun_path[rundir_size + sizeof (char)], sockname,
          sockname_size);
  addr.sun_path[rundir_size + sizeof (char) + sockname_size] = '\0';

  int ret = connect (sockfd, (const struct sockaddr *) &addr, sizeof (addr));
  assert (ret == 0);

  // init struct

  // rw socket
  io->fd = sockfd;

  // reader state
  io->next_frame = 0;
  io->read_end = 0;
}

void
wlw_send (wlw_io_state *io, wlw_msg_view *msg)
{
  wlw_msg_size size = msg->hdr.size_opcode >> 16;

#if 0
  wlw_msg_len len = wlw_size_to_len (size);
  printf ("C->S (%d)", size);
  for (wlw_msg_len i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *) msg)[i]);
    }
  printf ("\n");
#endif

  write (io->fd, msg, size);
}

wlw_msg_view *
wlw_recv (wlw_io_state *io)
{
  wlw_msg_view *msg;
  wlw_msg_size remainder;
retry:
  msg = (wlw_msg_view *) &io->buf[io->next_frame];
  remainder = io->read_end - io->next_frame;

  if (remainder < sizeof (wlw_header))
    goto refill_and_retry;

  wlw_msg_size size = msg->hdr.size_opcode >> 16;

  assert (size < sizeof (io->buf));
  if (remainder < size)
    goto refill_and_retry;

  io->next_frame += size;

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

#if 0
  wlw_msg_len len = wlw_size_to_len (size);
  printf ("S->C (%d)", size);
  for (wlw_msg_len i = 0; i < len; i++)
    {
      printf (" %08x", ((wlw_word *) msg)[i]);
    }
  printf ("\n");
#endif

  return msg;

  // this should only ever hit atmost once per call. this should not ever loop
refill_and_retry:
  // compact
  memmove (&io->buf[0], &io->buf[io->next_frame], remainder);
  io->read_end = remainder;
  io->next_frame = 0;
  // read
  int n =
    read (io->fd, &io->buf[io->read_end], sizeof (io->buf) - io->read_end);
  assert (n > 0);
  io->read_end += n;
  // retry
  goto retry;
}

static inline wlw_uint *
wlw_read_uint (wlw_word **head)
{
  return (wlw_uint *) (*head)++;
}

static inline wlw_object *
wlw_read_object (wlw_word **head)
{
  return (wlw_object *) (*head)++;
}

static inline wlw_static_new_id *
wlw_read_static_new_id (wlw_word **head)
{
  return (wlw_static_new_id *) (*head)++;
}

static inline wlw_string *
wlw_read_string (wlw_word **head)
{
  wlw_string *ret = (wlw_string *) (*head);

  (*head)++;                    // skip the length field
  // align to wlw_word boundary
  *head += wlw_size_to_len (ret->len);
  return ret;
}

static inline void
wlw_write_uint (wlw_word **head, wlw_uint *val)
{
  **head = *val;
  (*head)++;
}

static inline void
wlw_write_object (wlw_word **head, wlw_object *obj)
{
  **head = obj->id;
  (*head)++;
}

static inline void
wlw_write_static_new_id (wlw_word **head, wlw_static_new_id *id)
{
  **head = id->repr;
  (*head)++;
}

static inline void
wlw_write_string (wlw_word **head, wlw_string *str)
{
  **head = str->len;
  (*head)++;                    // skip the length field

  memcpy (*head, str->str, str->len);
  // align to wlw_word boundary
  *head += wlw_size_to_len (str->len);
}

static inline void
wlw_write_dynamic_new_id (wlw_word **head, wlw_string *interface,
                          wlw_uint *version, wlw_static_new_id *id)
{
  wlw_write_string (head, interface);
  wlw_write_uint (head, version);
  wlw_write_static_new_id (head, id);
}

#endif // WLW_IMPLEMENTATION

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include <stdio.h>

#include "wlw.h"

#define WLW_INTERFACES(X)                                                        \
  X (_wlw_null)                                                                  \
  X (wl_display)                                                                 \
  X (wl_registry)                                                                \
  X (wl_callback)                                                                \
  X (wl_compositor)                                                              \
  X (wl_surface)                                                                 \
  X (xdg_wm_base)                                                                \
  X (xdg_surface)                                                                \
  X (xdg_toplevel)                                                               \


typedef enum
{
#define X(name) name##_i,
  WLW_INTERFACES (X)
#undef X
    _wlw_i_size,
} wlw_interface;

// *INDENT-OFF*
#define X(name) \
 static const struct {wlw_word len; const char str[sizeof(#name)]; } \
   name##_i_s = { .len = sizeof(#name), .str = #name };
WLW_INTERFACES (X)
#undef X
// *INDENT-ON*

wlw_string *wlw_interface_names[] = {
#define X(name) (wlw_string*) &name##_i_s,
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

wlw_static_new_id
wlw_bookkeep_obj_genid (wlw_obj_map *obj_map)
{
  wlw_static_new_id new_id = { ++obj_map->free_tail };
  assert (new_id.repr < WLW_MAX_OBJECT_COUNT);
  return new_id;
}

wlw_object
wlw_bookkeep_obj_bind (wlw_obj_map *obj_map, wlw_interface interface,
                       wlw_static_new_id new_id)
{
  wlw_object obj = { new_id.repr };
  obj_map->items[obj.id] = interface;
  return obj;
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
  *wlw_null_obj = wlw_bookkeep_obj_bind (obj_map, _wlw_null_i,
                                         wlw_bookkeep_obj_genid (obj_map));
  *wl_display = wlw_bookkeep_obj_bind (obj_map, wl_display_i,
                                       wlw_bookkeep_obj_genid (obj_map));
}

typedef struct
{
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
                         wlw_static_new_id registry)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id registry;
  } msg;
  msg.hdr.object = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_get_registry;
  msg.registry = registry;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, wl_registry_i, registry);
}

wlw_object
wl_display_sync (wlw_state *wlw, wlw_object wl_display,
                 wlw_static_new_id callback)
{
  assert (wl_display.id == 1);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id callback;
  } msg;
  msg.hdr.object = wl_display;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_display_r_sync;
  msg.callback = callback;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg.hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, wl_callback_i, callback);
}

enum _wl_registry_r
{
  _wl_registry_r_bind,
};
enum _wl_registry_e
{
  _wl_registry_e_global,
};

void
wl_registry_global (wlw_state *wlw, wlw_msg_view *msg, wlw_uint **out_name,
                    wlw_string **out_interface, wlw_uint **out_version)
{
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, msg->hdr.object)
          == wl_registry_i);
  uint16_t opcode = msg->hdr.size_opcode & ((1 << 16) - 1);
  assert (opcode == _wl_registry_e_global);

  wlw_word *head = msg->payload;

  *out_name = wlw_read_uint (&head);
  *out_interface = wlw_read_string (&head);
  *out_version = wlw_read_uint (&head);
}

wlw_object
wl_registry_bind (wlw_state *wlw, wlw_object wl_registry, wlw_uint name,
                  wlw_interface interface, wlw_uint version,
                  wlw_static_new_id id)
{
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, wl_registry) ==
          wl_registry_i);
  wlw_word message[16] = { 0 };
  wlw_msg_view *msg = (void *) &message;

  wlw_word *head = msg->payload;
  wlw_write_uint (&head, &name);
  wlw_write_dynamic_new_id (&head, wlw_interface_names[interface], &version,
                            &id);

  wlw_msg_size size = ((wlw_byte *) head - (wlw_byte *) msg);
  msg->hdr.object = wl_registry;
  msg->hdr.size_opcode = size << 16 | _wl_registry_r_bind;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg->hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, interface, id);
}

enum _wl_compositor_r
{
  _wl_compositor_r_create_surface,
};

wlw_object
wl_compositor_create_surface (wlw_state *wlw, wlw_object wl_compositor,
                              wlw_static_new_id id)
{
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, wl_compositor) ==
          wl_compositor_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.hdr.object = wl_compositor;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _wl_compositor_r_create_surface;
  msg.id = id;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg.hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, wl_surface_i, id);
}

enum _xdg_wm_base_r
{
  _xdg_wm_base_r_destroy,
  _xdg_wm_base_r_create_positioner,
  _xdg_wm_base_r_get_xdg_surface,
  _xdg_wm_base_r_pong,
};

enum _xdg_wm_base_e
{
  _xdg_wm_base_e_ping,
};

wlw_object
xdg_wm_base_get_xdg_surface (wlw_state *wlw, wlw_object xdg_wm_base,
                             wlw_static_new_id id, wlw_object wl_surface)
{
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, xdg_wm_base) ==
          xdg_wm_base_i);
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, wl_surface) ==
          wl_surface_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
    wlw_object surface;
  } msg;
  msg.hdr.object = xdg_wm_base;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _xdg_wm_base_r_get_xdg_surface;
  msg.id = id;
  msg.surface = wl_surface;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg.hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, xdg_surface_i, id);
}

enum _xdg_surface_r
{
  _xdg_surface_r_destroy,
  _xdg_surface_r_get_toplevel,
};

wlw_object
xdg_surface_get_toplevel (wlw_state *wlw, wlw_object xdg_surface,
                          wlw_static_new_id id)
{
  assert (wlw_bookkeep_obj_typeof (&wlw->obj_map, xdg_surface) ==
          xdg_surface_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.hdr.object = xdg_surface;
  msg.hdr.size_opcode = sizeof (msg) << 16 | _xdg_surface_r_get_toplevel;
  msg.id = id;
  wlw_send (&wlw->io, (wlw_msg_view *) &msg.hdr);
  return wlw_bookkeep_obj_bind (&wlw->obj_map, xdg_toplevel_i, id);
}


int
main ()
{
  wlw_state wlw = { 0 };
  wlw_open (&wlw.io);
  wlw_object wlw_null_obj, wl_display;
  wlw_bookkeep_obj_setup_reserved (&wlw.obj_map, &wlw_null_obj, &wl_display);

  wl_display_get_registry (&wlw, wl_display,
                           wlw_bookkeep_obj_genid (&wlw.obj_map));
  wl_display_sync (&wlw, wl_display, wlw_bookkeep_obj_genid (&wlw.obj_map));

  wlw_object wl_surface = wlw_null_obj;
  bool running = true;
  while (running)
    {
      wlw_msg_view *msg = wlw_recv (&wlw.io);
      switch (wlw_bookkeep_obj_typeof (&wlw.obj_map, msg->hdr.object))
        {
        case wl_registry_i:
          assert ((msg->hdr.size_opcode & ((1 << 16) - 1))
                  == _wl_registry_e_global);

          wlw_uint *name;
          wlw_string *interface;
          wlw_uint *version;
          wl_registry_global (&wlw, msg, &name, &interface, &version);
          // printf ("global(%d) = %s@%d\n", *name, interface->str, *version);

          if (strcmp (wl_compositor_i_s.str, interface->str) == 0)
            {
              wlw_object wl_compositor =
                wl_registry_bind (&wlw, msg->hdr.object, *name,
                                  wl_compositor_i,
                                  *version,
                                  wlw_bookkeep_obj_genid (&wlw.obj_map));
              wl_surface =
                wl_compositor_create_surface (&wlw, wl_compositor,
                                              wlw_bookkeep_obj_genid
                                              (&wlw.obj_map));
            }
          else if (strcmp (xdg_wm_base_i_s.str, interface->str) == 0)
            {
              wlw_object wm_base =
                wl_registry_bind (&wlw, msg->hdr.object, *name,
                                  xdg_wm_base_i,
                                  *version,
                                  wlw_bookkeep_obj_genid (&wlw.obj_map));
              wlw_object xdg_surface =
                xdg_wm_base_get_xdg_surface (&wlw, wm_base,
                                             wlw_bookkeep_obj_genid
                                             (&wlw.obj_map),
                                             wl_surface);
              (void) xdg_surface;
            }

          break;
        case wl_callback_i:
          wlw_bookkeep_obj_unbind (&wlw.obj_map, msg->hdr.object);
          running = false;
          break;
        default:
          wlw_interface type =
            wlw_bookkeep_obj_typeof (&wlw.obj_map, msg->hdr.object);
          printf ("[!!!] %s:", wlw_interface_names[type]->str);
          printf (" %s\n", (char *) &msg->payload[3]);
          exit (0);
          for (wlw_msg_len i = 0;
               i < wlw_size_to_len (msg->hdr.size_opcode >> 16); i++)
            {
              printf (" %08x", ((wlw_word *) msg)[i]);
            }
          printf ("\n");
        };
    }
}

#define WLW_IMPLEMENTATION
#include "wlw.h"
#endif
