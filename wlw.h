#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdio.h>

#ifndef _WLW_H
#define _WLW_H

/*
  Commentary: a blocking wayland client tha treats the compositor as
  synchronous, trusted and known
*/

#ifndef NDEBUG
#define wlw_assert(cond, ...) _wlw_assert1(cond, __FILE__, __LINE__, __VA_ARGS__)
#define _wlw_assert1(cond, file, line, ...) _wlw_assert2(cond, file, line, __VA_ARGS__)
#define _wlw_assert2(cond, file, line, ...)				   \
  do									   \
    {									   \
      if (!(cond))							   \
	{								   \
	  char str[] = file ":" #line ": error: " __VA_ARGS__ "\n";	   \
	  write (2, str, sizeof (str) - 1);				   \
	  _exit (1);							   \
	}								   \
    }									   \
  while (0)
#else
#define wlw_assert(cond, ...) (void)(cond)
#endif // NDEBUG

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
  wlw_word size;
  const wlw_word untyped[];
} wlw_array;

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
static void wlw_print_msg (const wlw_msg_view *msg);

static inline wlw_size wlw_msg_opcode (const wlw_msg_view *hdr);
static inline uint16_t wlw_msg_size (const wlw_msg_view *hdr);
static inline wlw_word wlw_size_opcode (wlw_size size, uint16_t opcode);
// dynamically sized, need to read from head to get size
static inline const wlw_string *wlw_read_string (const wlw_word **head);

// same as reader pattern
static inline void wlw_write_string (wlw_word **head, wlw_word len,
                                     const char *str);

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
const wlw_msg_view *wlw_recv_until_sync (wl_callback *callback);

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

// wl_surface
void wl_surface_r_commit (wl_surface self);

// xdg_wm_base
xdg_surface xdg_wm_base_r_get_xdg_surface (xdg_wm_base self,
                                           wlw_static_new_id id,
                                           wl_surface surface);

// xdg_surface
xdg_toplevel xdg_surface_r_get_toplevel (xdg_surface self,
                                         wlw_static_new_id id);
typedef struct
{
  wlw_uint serial;
} xdg_surface_configure_serial;
xdg_surface_configure_serial xdg_surface_e_configure (const wlw_msg_view
                                                      *msg);
void xdg_surface_r_ack_configure (xdg_surface self,
                                  xdg_surface_configure_serial serial);

// xdg_toplevel

// trailing dynamic member, we can just cast pointers from the reader
typedef struct
{
  wlw_uint width;
  wlw_uint height;
  // note: use an inline enum here the day is start caring for this field
  wlw_array states;
} xdg_toplevel_e_configure_args;
const xdg_toplevel_e_configure_args *xdg_toplevel_e_configure (const
                                                               wlw_msg_view
                                                               *msg);
// note: dont care for the response, skip for now
void xdg_toplevel_e_wm_capabilities (const wlw_msg_view *msg);


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
  wlw_assert (wlw_sockfd == -1, "sockfd already initialised");
  wlw_sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  wlw_assert (wlw_sockfd >= 0, "os error, recompile with perror");

  struct sockaddr_un addr = {
    .sun_family = AF_UNIX,
    .sun_path = "/run/user/1000/wayland-1",
  };

  int ret =
    connect (wlw_sockfd, (const struct sockaddr *) &addr, sizeof (addr));
  wlw_assert (ret == 0, "os error, recompile with perror");
}

void
wlw_send (const wlw_msg_view *msg)
{
  wlw_size size = wlw_msg_size (msg);
  int n = write (wlw_sockfd, msg, size);
  wlw_assert (n == size, "partial message written");
}

static inline int
wlw__recv_refill ()
{
  wlw_size remainder = wlw_reader_end - wlw_reader_next;

  wlw_assert (remainder <= wlw_reader_next, "not enough space to compact");
  memcpy (&wlw_reader_buf[0], &wlw_reader_buf[wlw_reader_next], remainder);
  wlw_reader_end = remainder;
  wlw_reader_next = 0;

  int n = read (wlw_sockfd, &wlw_reader_buf[wlw_reader_end],
                sizeof (wlw_reader_buf) - wlw_reader_end);
  wlw_assert (n > 0, "os error, recompile with perror");
  wlw_reader_end += n;

  return n;
}

const wlw_msg_view *
wlw_recv ()
{
  wlw_size remainder = wlw_reader_end - wlw_reader_next;
  wlw_msg_view *msg = (wlw_msg_view *) &wlw_reader_buf[wlw_reader_next];

  if (remainder < sizeof (wlw_header))
    {
      remainder += wlw__recv_refill ();
      msg = (wlw_msg_view *) &wlw_reader_buf[wlw_reader_next];
    }

  wlw_size size = wlw_msg_size (msg);
  wlw_assert (size < sizeof (wlw_reader_buf), "message too large");

  if (remainder < size)
    {
      remainder += wlw__recv_refill ();
      msg = (wlw_msg_view *) &wlw_reader_buf[wlw_reader_next];
    }

  wlw_reader_next += wlw_msg_size (msg);
  return msg;
}

static void
wlw_print_msg (const wlw_msg_view *msg)
{
  wlw_size size = wlw_msg_size (msg) - sizeof (wlw_header);
  printf ("%s(%d): !%d [%d] ",
          wlw_interface_names[wlw_obj_typeof (msg->hdr.object)].str,
          msg->hdr.object.id, wlw_msg_opcode (msg), size);
  for (wlw_len i = 0; i < wlw_size_to_len (size); i++)
    {
      printf ("#x%08x ", ((wlw_word *) &msg->payload)[i]);
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

static inline wlw_word
wlw_size_opcode (wlw_size size, uint16_t opcode)
{
  return (size << 16) | opcode;
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
wlw_write_string (wlw_word **head, wlw_word len, const char *str)
{
  **head = len;
  (*head)++;                    // skip the length field

  memcpy (*head, str, len);
  // align to wlw_word boundary
  *head += wlw_size_to_len (len);
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
  wlw_assert (new_id.repr < (sizeof (wlw_obj_map) / sizeof (wlw_obj_map[0])),
              "ran out of ids");
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

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), _wl_display_r_get_registry);
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

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode = wlw_size_opcode (sizeof (msg), _wl_display_r_sync);
  wlw_send ((wlw_msg_view *) &msg);

  wl_callback ret = { wlw_obj_bind (wl_callback_i, callback) };
  return ret;
}

void
wl_display_e_delete_id (const wlw_msg_view *msg)
{
  wlw_assert (msg->hdr.object.id == 1);
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_display_i, "bad opcode");

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
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_callback_i,
              "bad object");
  wlw_assert (wlw_msg_opcode (msg) == _wl_callback_e_done, "bad opcode");

  wl_callback_args args = {
    .callback_data = msg->payload[0],
  };
  return args;
}

const wlw_msg_view *
wlw_recv_until_sync (wl_callback *callback)
{
  static bool is_callback_pending = false;
  if (callback->as_obj.id == wlw_object_invalid.id)
    {
      wlw_assert (!is_callback_pending,
                  "attempted to sync while another callback is pending");
      is_callback_pending = true;
      *callback = wl_display_r_sync (wlw_obj_genid ());
    }
  wlw_assert (wlw_obj_typeof (callback->as_obj) == wl_callback_i,
              "not a sync point");

  const wlw_msg_view *msg = wlw_recv ();

  if (msg->hdr.object.id == callback->as_obj.id)
    {
      (void) wl_callback_e_done (msg);
      wlw_assert (is_callback_pending, "unexpected callback completion");
      is_callback_pending = false;

      wl_display_e_delete_id (wlw_recv ());
      callback->as_obj = wlw_object_invalid;
      return NULL;
    }
  return msg;
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
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_registry_i,
              "bad object");
  wlw_assert (wlw_msg_opcode (msg) == _wl_registry_e_global, "bad opcode");

  const wlw_word *head = msg->payload;

  wl_registry_e_global_args args;
  args.name = *(head++);
  args.interface = wlw_read_string (&head);
  args.version = *(head++);
  return args;
}

wlw_object
wl_registry_r_bind (wl_registry self, wlw_uint name,
                    wlw_interface interface, wlw_uint version,
                    wlw_static_new_id id)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_registry_i);

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
  *(head++) = name;
  // dynamic new id
  wlw_write_string (&head, wlw_interface_names[interface].len,
                    wlw_interface_names[interface].str);
  *(head++) = version;
  *(head++) = id.repr;

  wlw_size size = ((wlw_byte *) head - (wlw_byte *) msg);
  msg->hdr.object = self.as_obj;
  msg->hdr.size_opcode = wlw_size_opcode (size, _wl_registry_r_bind);
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
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_compositor_i);

  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.id = id;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), _wl_compositor_r_create_surface);
  wlw_send ((wlw_msg_view *) &msg);

  wl_surface ret = { wlw_obj_bind (wl_surface_i, id) };
  return ret;
}


// wl_surface

enum _wl_surface_opcodes
{
  _wl_surface_r_destroy = 0,
  _wl_surface_r_attach,
  _wl_surface_r_damage,         // looks deprecated
  _wl_surface_r_frame,
  _wl_surface_r_set_opaque_region,
  _wl_surface_r_set_input_region,
  _wl_surface_r_commit,
};

void
wl_surface_r_commit (wl_surface self)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_surface_i);

  struct
  {
    wlw_header hdr;
  } msg;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode = wlw_size_opcode (sizeof (msg), _wl_surface_r_commit);
  wlw_send ((wlw_msg_view *) &msg);
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
  wlw_assert (wlw_obj_typeof (self.as_obj) == xdg_wm_base_i);
  wlw_assert (wlw_obj_typeof (surface.as_obj) == wl_surface_i,
              "bad argument");
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
    wl_surface surface;
  } msg;
  msg.id = id;
  msg.surface = surface;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), _xdg_wm_base_r_get_xdg_surface);
  wlw_send ((wlw_msg_view *) &msg);

  xdg_surface ret = { wlw_obj_bind (xdg_surface_i, id) };
  return ret;
}


// xdg_surface

enum _xdg_surface_r
{
  _xdg_surface_r_destroy = 0,
  _xdg_surface_r_get_toplevel,
  _xdg_surface_r_get_popup,
  _xdg_surface_r_set_window_geometry,
  _xdg_surface_r_ack_configure,
  _xdg_surface_e_configure = 0,
};

xdg_toplevel
xdg_surface_r_get_toplevel (xdg_surface self, wlw_static_new_id id)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == xdg_surface_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.id = id;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), _xdg_surface_r_get_toplevel);
  wlw_send ((wlw_msg_view *) &msg);

  xdg_toplevel ret = { wlw_obj_bind (xdg_toplevel_i, id) };
  return ret;
}

xdg_surface_configure_serial
xdg_surface_e_configure (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_surface_i,
              "bad object");
  wlw_assert (wlw_msg_opcode (msg) == _xdg_surface_e_configure, "bad opcode");

  xdg_surface_configure_serial ret = {
    .serial = msg->payload[0],
  };
  return ret;
}

void
xdg_surface_r_ack_configure (xdg_surface self,
                             xdg_surface_configure_serial serial)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == xdg_surface_i);

  struct
  {
    wlw_header hdr;
    xdg_surface_configure_serial serial;
  } msg;
  msg.serial = serial;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), _xdg_surface_r_ack_configure);
  wlw_send ((wlw_msg_view *) &msg);
}

// xdg_toplevel
enum _xdg_toplevel_opcodes
{
  _xdg_toplevel_e_configure = 0,
  _xdg_toplevel_e_close,
  _xdg_toplevel_e_configure_bounds,
  _xdg_toplevel_e_wm_capabilities,
};

const xdg_toplevel_e_configure_args *
xdg_toplevel_e_configure (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_toplevel_i);
  wlw_assert (wlw_msg_opcode (msg) == _xdg_toplevel_e_configure);

  // trailing dynamic member, castable to flexible member
  return (xdg_toplevel_e_configure_args *) msg->payload;
}

void
xdg_toplevel_e_wm_capabilities (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_toplevel_i);
  wlw_assert (wlw_msg_opcode (msg) == _xdg_toplevel_e_wm_capabilities);
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
  const wlw_msg_view *msg;
  wl_callback syncpoint = { wlw_object_invalid };

  while ((msg = wlw_recv_until_sync (&syncpoint)))
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
  wl_surface_r_commit (surface);
  xdg_toplevel_e_wm_capabilities (wlw_recv ());
  const xdg_toplevel_e_configure_args *args =
    xdg_toplevel_e_configure (wlw_recv ());

  xdg_surface_r_ack_configure (shell_surface,
                               xdg_surface_e_configure (wlw_recv ()));
  printf ("window w=%d * h=%d\n", args->width, args->height);

  printf ("todo list:\n");
  while ((msg = wlw_recv_until_sync (&syncpoint)))
    wlw_print_msg (msg);
}

#endif
