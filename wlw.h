#include <stdio.h>
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

// declarations

typedef uint32_t wlw_word;
typedef uint8_t wlw_byte;

// taxonomy note,
// "size" = in bytes
// "len" = count in array type, in this case wlw_word

// spec defines that size always fits top 16 bits of (wlw_header.size_opcode)
typedef uint16_t wlw_size;
// only bottom 14 bits should ever be used,
// 16 - log2(sizeof(wlw_word))
typedef uint16_t wlw_len;
static inline wlw_len wlw_size_to_len (wlw_size size);


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
  const char str[];
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


void wlw_open ();
void wlw_send (const wlw_msg_view *msg);
const wlw_msg_view *wlw_recv ();
// debug
static inline void wlw_print_msg (const wlw_msg_view *msg);

static inline wlw_size wlw_msg_opcode (const wlw_msg_view *hdr);
static inline uint16_t wlw_msg_size (const wlw_msg_view *hdr);
static inline void wlw_hdr_prepare (wlw_header *hdr,
                                    wlw_object obj,
                                    uint16_t opcode, wlw_size size);
// returned pointer is always same as dereferencing what was passed in
// the only utility of these functions is that they advance the head
static inline wlw_uint wlw_read_uint (const wlw_word **head);
static inline wlw_object wlw_read_object (const wlw_word **head);
static inline wlw_static_new_id wlw_read_static_new_id (const wlw_word
                                                        **head);
// dynamically sized, need to read from head to get size
static inline const wlw_string *wlw_read_string (const wlw_word **head);

// same as reader pattern
static inline void wlw_write_uint (wlw_word **head, wlw_uint val);
static inline void wlw_write_object (wlw_word **head, wlw_object obj);
static inline void wlw_write_static_new_id (wlw_word **head,
                                            wlw_static_new_id id);
static inline void wlw_write_string (wlw_word **head, wlw_word len,
                                     const char *str);
// composite writer, used often enough
static inline void wlw_write_dynamic_new_id (wlw_word **head,
                                             wlw_word interface_len,
                                             const char *interface_str,
                                             wlw_uint version,
                                             wlw_static_new_id id);


// interfaces
typedef enum
{
  _wlw_i_null,
  wl_display_i,
  wl_callback_i,
  wl_registry_i,
  wl_compositor_i,
  wl_surface_i,
  xdg_wm_base_i,
  xdg_surface_i,
  xdg_toplevel_i,
  _wlw_i_size,
} wlw_interface;

static const struct
{
  const wlw_word len;
  const char str[16];
} wlw_interface_names[] = {
  [_wlw_i_null] = { sizeof ("*invalid*"), "*invalid*" },
  [wl_display_i] = { sizeof ("wl_display"), "wl_display" },
  [wl_callback_i] = { sizeof ("wl_callback"), "wl_callback" },
  [wl_registry_i] = { sizeof ("wl_registry"), "wl_registry" },
  [wl_compositor_i] = { sizeof ("wl_compositor"), "wl_compositor" },
  [wl_surface_i] = { sizeof ("wl_surface"), "wl_surface" },
  [xdg_wm_base_i] = { sizeof ("xdg_wm_base"), "xdg_wm_base" },
  [xdg_surface_i] = { sizeof ("xdg_surface"), "xdg_surface" },
  [xdg_toplevel_i] = { sizeof ("xdg_toplevel"), "xdg_toplevel" },
};

_Static_assert (sizeof (wlw_interface_names) /
                sizeof (wlw_interface_names[0]) == _wlw_i_size,
                "wlw_interface_names out of sync with wlw_interface");

static const wlw_object wlw_object_invalid = { 0 };
// *INDENT-OFF*
typedef struct { wlw_object as_obj; } wl_display;
typedef struct { wlw_object as_obj; } wl_callback;
typedef struct { wlw_object as_obj; } wl_registry;
typedef struct { wlw_object as_obj; } wl_compositor;
typedef struct { wlw_object as_obj; } wl_surface;
typedef struct { wlw_object as_obj; } xdg_wm_base;
typedef struct { wlw_object as_obj; } xdg_surface;
typedef struct { wlw_object as_obj; } xdg_toplevel;
// *INDENT-ON*

// bookkeeping
#define WLW_MAX_OBJECT_COUNT 32
wlw_static_new_id wlw_obj_genid ();
wlw_object wlw_obj_bind (wlw_interface interface, wlw_static_new_id new_id);
void wlw_obj_unbind (wlw_object object);
wlw_interface wlw_obj_typeof (wlw_object object);

// protocols

// wl_display
wl_registry wl_display_r_get_registry (wlw_static_new_id registry);
wl_callback wl_display_r_sync (wlw_static_new_id callback);
void wl_display_e_delete_id (const wlw_msg_view *msg);

// wl_callback
typedef struct
{
  wlw_uint callback_data;
} wl_callback_args;
wl_callback_args wl_callback_e_done (const wlw_msg_view *msg);
bool wlw_recv_until_sync (wlw_msg_view **msg, wl_callback *callback);

// wl_registry
wlw_object wl_registry_r_bind (wl_registry self, wlw_uint name,
                               wlw_interface id_interface,
                               wlw_uint id_version, wlw_static_new_id id_id);
typedef struct
{
  wlw_uint name;
  const wlw_string *interface;
  wlw_uint version;
} wl_registry_e_global_args;
wl_registry_e_global_args wl_registry_e_global (const wlw_msg_view *msg);

// wl_compositor
wl_surface wl_compositor_r_create_surface (wl_compositor self,
                                           wlw_static_new_id id);

// xdg_wm_base
xdg_surface xdg_wm_base_r_get_xdg_surface (xdg_wm_base self,
                                           wlw_static_new_id id,
                                           wl_surface surface);

// xdg_surface
xdg_toplevel xdg_surface_r_get_toplevel (xdg_surface self,
                                         wlw_static_new_id id);

// global helpers

static inline wlw_len
wlw_size_to_len (wlw_size size)
{
  return (size + sizeof (wlw_word) - 1) / sizeof (wlw_word);
}

// io

int wlw_sockfd = -1;
wlw_size wlw_reader_next = 0;
wlw_size wlw_reader_end = 0;
alignas (wlw_word)
     wlw_byte wlw_reader_buf[256 *sizeof (wlw_word)] = { 0 };

void
wlw_open ()
{
  assert (wlw_sockfd == -1);
  wlw_sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  assert (wlw_sockfd >= 0);

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

  int ret =
    connect (wlw_sockfd, (const struct sockaddr *) &addr, sizeof (addr));
  assert (ret == 0);
}

void
wlw_send (const wlw_msg_view *msg)
{
  wlw_size size = wlw_msg_size (msg);
  int n = write (wlw_sockfd, msg, size);
  assert (n == size);
}

static inline int
_wlw_recv_refill ()
{
  wlw_size remainder = wlw_reader_end - wlw_reader_next;

  assert (remainder <= wlw_reader_next);
  memcpy (&wlw_reader_buf[0], &wlw_reader_buf[wlw_reader_next], remainder);
  wlw_reader_end = remainder;
  wlw_reader_next = 0;

  int n = read (wlw_sockfd, &wlw_reader_buf[wlw_reader_end],
                sizeof (wlw_reader_buf) - wlw_reader_end);
  assert (n > 0);
  wlw_reader_end += n;

  return n;
}

// warn: does not guarantee a full message has been read
static inline const wlw_msg_view *
_wlw_recv_peek ()
{
  return ((wlw_msg_view *) &wlw_reader_buf[wlw_reader_next]);
}

static inline const wlw_msg_view *
_wlw_recv_advance ()
{
  const wlw_msg_view *msg = _wlw_recv_peek ();
  assert (wlw_reader_next + wlw_msg_size (msg) <= wlw_reader_end);
  wlw_reader_next += wlw_msg_size (msg);
  return msg;
}

const wlw_msg_view *
wlw_recv ()
{
  wlw_size remainder = wlw_reader_end - wlw_reader_next;

  if (remainder < sizeof (wlw_header))
    remainder += _wlw_recv_refill ();

  wlw_size size = wlw_msg_size (_wlw_recv_peek ());
  assert (size < sizeof (wlw_reader_buf));
  if (remainder < size)
    remainder += _wlw_recv_refill ();

  return _wlw_recv_advance ();
}

// debug
static inline void
wlw_print_msg (const wlw_msg_view *msg)
{
  wlw_size size = wlw_msg_size (msg);
  printf ("%s(%d): !%d [%d] ",
          wlw_interface_names[wlw_obj_typeof (msg->hdr.object)].str,
          msg->hdr.object.id, wlw_msg_opcode (msg), size);
  for (wlw_len i = 0; i < wlw_size_to_len (size); i++)
    {
      printf ("%08x ", ((wlw_word *) &msg->payload)[i]);
    }
  printf ("\n");
}


// wire types

static inline uint16_t
wlw_msg_opcode (const wlw_msg_view *msg)
{
  return (msg->hdr.size_opcode >> 0) & UINT16_MAX;
}

static inline wlw_size
wlw_msg_size (const wlw_msg_view *msg)
{
  return (msg->hdr.size_opcode >> 16) & UINT16_MAX;
}

static inline void
wlw_hdr_prepare (wlw_header *hdr,
                 wlw_object obj, uint16_t opcode, wlw_size size)
{
  hdr->object = obj;
  hdr->size_opcode = (size << 16) | opcode;
}

static inline wlw_uint
wlw_read_uint (const wlw_word **head)
{
  wlw_uint ret = **head;
  (*head)++;
  return ret;
}

static inline wlw_object
wlw_read_object (const wlw_word **head)
{
  wlw_object ret = {.id = **head };
  (*head)++;
  return ret;
}

static inline wlw_static_new_id
wlw_read_static_new_id (const wlw_word **head)
{
  wlw_static_new_id ret = {.repr = **head };
  (*head)++;
  return ret;
}

static inline const wlw_string *
wlw_read_string (const wlw_word **head)
{
  wlw_string *ret = (wlw_string *) (*head);

  (*head)++;                    // skip the length field
  // align to wlw_word boundary
  *head += wlw_size_to_len (ret->len);
  return ret;
}

static inline void
wlw_write_uint (wlw_word **head, wlw_uint val)
{
  **head = val;
  (*head)++;
}

static inline void
wlw_write_object (wlw_word **head, wlw_object obj)
{
  **head = obj.id;
  (*head)++;
}

static inline void
wlw_write_static_new_id (wlw_word **head, wlw_static_new_id id)
{
  **head = id.repr;
  (*head)++;
}

static inline void
wlw_write_string (wlw_word **head, wlw_word len, const char *str)
{
  **head = len;
  (*head)++;                    // skip the length field

  memcpy (*head, str, len);
  // align to wlw_word boundary
  *head += wlw_size_to_len (len);
}

static inline void
wlw_write_dynamic_new_id (wlw_word **head,
                          wlw_word interface_len,
                          const char *interface_str,
                          wlw_uint version, wlw_static_new_id id)
{
  wlw_write_string (head, interface_len, interface_str);
  wlw_write_uint (head, version);
  wlw_write_static_new_id (head, id);
}

// bookkeeping

// just linear allocator for now, we can do free lists if we need it
// preinitialise with reserved
uint32_t wlw_obj_free_tail = 2;
wlw_interface wlw_obj_map[32] = {
  // not really used, but wl_display_i starts at 1
  _wlw_i_null,
  wl_display_i,
};

wlw_static_new_id
wlw_obj_genid ()
{
  wlw_static_new_id new_id = { wlw_obj_free_tail++ };
  assert (new_id.repr < (sizeof (wlw_obj_map) / sizeof (wlw_obj_map[0])));
  return new_id;
}

wlw_object
wlw_obj_bind (const wlw_interface interface, const wlw_static_new_id new_id)
{
  wlw_object obj = { new_id.repr };
  wlw_obj_map[obj.id] = interface;
  return obj;
}

void
wlw_obj_unbind (const wlw_object object)
{
  if (wlw_obj_free_tail == object.id)
    wlw_obj_free_tail--;
  // else give up, unless we decide to make this into a freelist
}

wlw_interface
wlw_obj_typeof (wlw_object object)
{
  return wlw_obj_map[object.id];
}

// wl_display

enum _wl_display_opcode
{
  _wl_display_r_sync = 0,
  _wl_display_r_get_registry,
  _wl_display_e_error = 0,
  _wl_display_e_delete_id,
};

wl_registry
wl_display_r_get_registry (wlw_static_new_id registry)
{
  wl_display self = { { 1 } };
  struct
  {
    wlw_header hdr;
    wlw_static_new_id registry;
  } msg;
  msg.registry = registry;

  wlw_hdr_prepare (&msg.hdr, self.as_obj, _wl_display_r_get_registry,
                   sizeof (msg));
  wlw_send ((wlw_msg_view *) &msg);

  wl_registry ret = { wlw_obj_bind (wl_registry_i, registry) };
  return ret;
}

wl_callback
wl_display_r_sync (wlw_static_new_id callback)
{
  wl_display self = { { 1 } };

  struct
  {
    wlw_header hdr;
    wlw_static_new_id callback;
  } msg;
  msg.callback = callback;

  wlw_hdr_prepare (&msg.hdr, self.as_obj, _wl_display_r_sync, sizeof (msg));
  wlw_send ((wlw_msg_view *) &msg);

  wl_callback ret = { wlw_obj_bind (wl_callback_i, callback) };
  return ret;
}

void
wl_display_e_delete_id (const wlw_msg_view *msg)
{
  assert (msg->hdr.object.id == 1);
  assert (wlw_obj_typeof (msg->hdr.object) == wl_display_i);

  wlw_object obj = { msg->payload[0] };
  wlw_obj_unbind (obj);
}

// wl_callback
enum _wl_callback_opcodes
{
  _wl_callback_e_done = 0,
};

wl_callback_args
wl_callback_e_done (const wlw_msg_view *msg)
{
  assert (wlw_obj_typeof (msg->hdr.object) == wl_callback_i);
  assert (wlw_msg_opcode (msg) == _wl_callback_e_done);

  wl_callback_args args = {
    .callback_data = msg->payload[0],
  };
  return args;
}

bool
wlw_recv_until_sync (wlw_msg_view **msg, wl_callback *callback)
{
  static bool is_callback_pending = false;
  if (callback->as_obj.id == wlw_object_invalid.id)
    {
      assert (!is_callback_pending);
      is_callback_pending = true;
      *callback = wl_display_r_sync (wlw_obj_genid ());
    }
  assert (wlw_obj_typeof (callback->as_obj) == wl_callback_i);

  *msg = (wlw_msg_view *) wlw_recv ();

  if ((*msg)->hdr.object.id == callback->as_obj.id)
    {
      (void) wl_callback_e_done (*msg);
      assert (is_callback_pending);
      is_callback_pending = false;

      wl_display_e_delete_id (wlw_recv ());
      callback->as_obj = wlw_object_invalid;
      return false;
    }
  return true;
}

// wl_registry
enum _wl_registry_opcodes
{
  _wl_registry_r_bind = 0,
  _wl_registry_e_global = 0,
};

wl_registry_e_global_args
wl_registry_e_global (const wlw_msg_view *msg)
{
  assert (wlw_obj_typeof (msg->hdr.object) == wl_registry_i);
  assert (wlw_msg_opcode (msg) == _wl_registry_e_global);

  const wlw_word *head = msg->payload;

  wl_registry_e_global_args args;
  args.name = wlw_read_uint (&head);
  args.interface = wlw_read_string (&head);
  args.version = wlw_read_uint (&head);
  return args;
}

wlw_object
wl_registry_r_bind (wl_registry self, wlw_uint name,
                    wlw_interface interface, wlw_uint version,
                    wlw_static_new_id id)
{
  assert (wlw_obj_typeof (self.as_obj) == wl_registry_i);

  // only for computing size
  typedef struct
  {
    wlw_header hdr;
    wlw_uint iname_len;
    // at max this length
    const char iname_str[sizeof (wlw_interface_names[0].str)];
    wlw_uint version;
    wlw_uint id;
  } _msg_layout_worst_case;
  wlw_word message[sizeof (_msg_layout_worst_case)];

  wlw_msg_view *msg = (wlw_msg_view *) &message;

  wlw_word *head = msg->payload;
  wlw_write_uint (&head, name);
  wlw_write_dynamic_new_id (&head,
                            wlw_interface_names[interface].len,
                            wlw_interface_names[interface].str, version, id);

  wlw_size size = ((wlw_byte *) head - (wlw_byte *) msg);
  wlw_hdr_prepare (&msg->hdr, self.as_obj, _wl_registry_r_bind, size);
  wlw_send (msg);
  return wlw_obj_bind (interface, id);
}

// wl_compositor
enum _wl_compositor_opcodes
{
  _wl_compositor_r_create_surface = 0,
};

wl_surface
wl_compositor_r_create_surface (wl_compositor self, wlw_static_new_id id)
{
  assert (wlw_obj_typeof (self.as_obj) == wl_compositor_i);

  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.id = id;

  wlw_hdr_prepare (&msg.hdr, self.as_obj, _wl_compositor_r_create_surface,
                   sizeof (msg));
  wlw_send ((wlw_msg_view *) &msg);

  wl_surface ret = { wlw_obj_bind (wl_surface_i, id) };
  return ret;
}

enum _xdg_wm_base_opcodes
{
  _xdg_wm_base_r_destroy = 0,
  _xdg_wm_base_r_create_positioner,
  _xdg_wm_base_r_get_xdg_surface,
  _xdg_wm_base_r_pong,
  _xdg_wm_base_e_ping = 0,
};

xdg_surface
xdg_wm_base_r_get_xdg_surface (xdg_wm_base self,
                               wlw_static_new_id id, wl_surface surface)
{
  assert (wlw_obj_typeof (self.as_obj) == xdg_wm_base_i);
  assert (wlw_obj_typeof (surface.as_obj) == wl_surface_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
    wl_surface surface;
  } msg;
  msg.id = id;
  msg.surface = surface;

  wlw_hdr_prepare (&msg.hdr, self.as_obj, _xdg_wm_base_r_get_xdg_surface,
                   sizeof (msg));
  wlw_send ((wlw_msg_view *) &msg);

  xdg_surface ret = { wlw_obj_bind (xdg_surface_i, id) };
  return ret;
}

// xdg_surface
enum _xdg_surface_r
{
  _xdg_surface_r_destroy,
  _xdg_surface_r_get_toplevel,
};

xdg_toplevel
xdg_surface_r_get_toplevel (xdg_surface self, wlw_static_new_id id)
{
  assert (wlw_obj_typeof (self.as_obj) == xdg_surface_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.id = id;

  wlw_hdr_prepare (&msg.hdr, self.as_obj, _xdg_surface_r_get_toplevel,
                   sizeof (msg));
  wlw_send ((wlw_msg_view *) &msg);

  xdg_toplevel ret = { wlw_obj_bind (xdg_toplevel_i, id) };
  return ret;
}

#endif // _WLW_H

#ifdef WLW_EXAMPLE
#undef WLW_EXAMPLE

#include "wlw.h"

int
main ()
{
  wlw_open ();

  wl_registry registry = wl_display_r_get_registry (wlw_obj_genid ());

  wl_compositor compositor = { wlw_object_invalid };
  xdg_wm_base wm_base = { wlw_object_invalid };
  wlw_msg_view *msg;
  wl_callback syncpoint = { wlw_object_invalid };

  while (wlw_recv_until_sync (&msg, &syncpoint))
    {
      wl_registry_e_global_args args = wl_registry_e_global (msg);
      const char *iname = args.interface->str;

      if (strcmp (iname, wlw_interface_names[wl_compositor_i].str) == 0)
        {
          compositor.as_obj =
            wl_registry_r_bind (registry, args.name, wl_compositor_i,
                                args.version, wlw_obj_genid ());
        }
      else if (strcmp (iname, wlw_interface_names[xdg_wm_base_i].str) == 0)
        {
          wm_base.as_obj =
            wl_registry_r_bind (registry, args.name, xdg_wm_base_i,
                                args.version, wlw_obj_genid ());
        }
    }

  wl_surface surface =
    wl_compositor_r_create_surface (compositor, wlw_obj_genid ());
  xdg_surface shell_surface =
    xdg_wm_base_r_get_xdg_surface (wm_base, wlw_obj_genid (), surface);
  xdg_toplevel toplevel =
    xdg_surface_r_get_toplevel (shell_surface, wlw_obj_genid ());
  (void) toplevel;

  printf ("todo list:\n");
  while (wlw_recv_until_sync (&msg, &syncpoint))
    wlw_print_msg (msg);
}

#endif
