#define _GNU_SOURCE
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/input.h>

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
#define _wlw_assert2(cond, file, line, ...)                                \
  do                                                                       \
    {                                                                      \
      if (!(cond))                                                         \
        {                                                                  \
          char str[] = file ":" #line ": error: " __VA_ARGS__ "\n";        \
          write (2, str, sizeof (str) - 1);                                \
          _exit (1);                                                       \
        }                                                                  \
    }                                                                      \
  while (0)
#else
#define wlw_assert(cond, ...) (void)(cond)
#endif // NDEBUG

// declarations

typedef uint32_t wlw_word;
typedef uint8_t wlw_byte;

// wire types
typedef wlw_word wlw_uint;

typedef enum _wlw_interface wlw_interface;
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
void wlw_send_with_fd (const wlw_msg_view *msg, int fd);
const wlw_msg_view *wlw_peek ();
static inline const wlw_msg_view *wlw_recv ();
static void wlw_print_msg (const wlw_msg_view *msg);


// spec defines that size always fits top 16 bits of (wlw_header.size_opcode)
typedef uint16_t wlw_size;
typedef uint16_t wlw_opcode;
// only bottom 14 bits should ever be used,
// 16 - log2(sizeof(wlw_word))
typedef uint16_t wlw_len;

static inline wlw_len wlw_size_to_len (wlw_size size);

static inline wlw_opcode wlw_msg_opcode (const wlw_msg_view *hdr);
static inline wlw_size wlw_msg_size (const wlw_msg_view *hdr);
static inline wlw_word wlw_size_opcode (wlw_size size, wlw_opcode opcode);
// dynamically sized, need to read from head to get size
static inline const wlw_string *wlw_read_string (const wlw_word **head);

// same as reader pattern
static inline void wlw_write_string (wlw_word **head, wlw_word len,
                                     const char *str);

// bookkeeping
wlw_static_new_id wlw_obj_genid ();
wlw_object wlw_obj_bind (wlw_interface interface, wlw_static_new_id new_id);
void wlw_obj_unbind (wlw_object object);
wlw_interface wlw_obj_typeof (wlw_object object);

enum _wlw_interface
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
  wl_seat_i,
  wl_keyboard_i,
  wl_shm_i,
  wl_shm_pool_i,
  wl_buffer_i,
  _wlw_i_size,
};

static const struct
{
  const wlw_word len;
  const char str[16];
} wlw_interface_names[] = {
  { sizeof ("*invalid*"), "*invalid*" },
  { sizeof ("wl_display"), "wl_display" },
  { sizeof ("wl_callback"), "wl_callback" },
  { sizeof ("wl_registry"), "wl_registry" },
  { sizeof ("wl_compositor"), "wl_compositor" },
  { sizeof ("wl_surface"), "wl_surface" },
  { sizeof ("xdg_wm_base"), "xdg_wm_base" },
  { sizeof ("xdg_surface"), "xdg_surface" },
  { sizeof ("xdg_toplevel"), "xdg_toplevel" },
  { sizeof ("wl_seat"), "wl_seat" },
  { sizeof ("wl_keyboard"), "wl_keyboard" },
  { sizeof ("wl_shm"), "wl_shm" },
  { sizeof ("wl_shm_pool"), "wl_shm_pool" },
  { sizeof ("wl_buffer"), "wl_buffer" },
};

_Static_assert ((sizeof (wlw_interface_names) /
                 sizeof (*wlw_interface_names)) == _wlw_i_size,
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
typedef struct { wlw_object as_obj; } wl_seat;
typedef struct { wlw_object as_obj; } wl_keyboard;
typedef struct { wlw_object as_obj; } wl_shm;
typedef struct { wlw_object as_obj; } wl_shm_pool;
typedef struct { wlw_object as_obj; } wl_buffer;
// *INDENT-ON*

enum _wlw_opcode
{
  // wl_display
  wl_display_r_sync_op = 0,
  wl_display_r_get_registry_op,
  wl_display_e_error_op = 0,
  wl_display_e_delete_id_op,
  // wl_callback
  wl_callback_e_done_op = 0,
  // wl_registry
  wl_registry_r_bind_op = 0,
  wl_registry_e_global_op = 0,
  // wl_compositor
  wl_compositor_r_create_surface_op = 0,
  // wl_surface
  wl_surface_r_destroy_op = 0,
  wl_surface_r_attach_op,
  wl_surface_r_damage_op,       // looks deprecated
  wl_surface_r_frame_op,
  wl_surface_r_set_opaque_region_op,
  wl_surface_r_set_input_region_op,
  wl_surface_r_commit_op,
  wl_surface_e_enter_op = 0,
  wl_surface_e_leave_op,
  wl_surface_e_preferred_buffer_scale_op,
  wl_surface_e_preferred_buffer_transform_op,
  // xdg_wm_base
  xdg_wm_base_r_destroy_op = 0,
  xdg_wm_base_r_create_positioner_op,
  xdg_wm_base_r_get_xdg_surface_op,
  xdg_wm_base_r_pong_op,
  xdg_wm_base_e_ping_op = 0,
  // xdg_surface
  xdg_surface_r_destroy_op = 0,
  xdg_surface_r_get_toplevel_op,
  xdg_surface_r_get_popup_op,
  xdg_surface_r_set_window_geometry_op,
  xdg_surface_r_ack_configure_op,
  xdg_surface_e_configure_op = 0,
  // xdg_toplevel
  xdg_toplevel_e_configure_op = 0,
  xdg_toplevel_e_close_op,
  xdg_toplevel_e_configure_bounds_op,
  xdg_toplevel_e_wm_capabilities_op,
  // wl_seat
  wl_seat_r_get_pointer_op = 0,
  wl_seat_r_get_keyboard_op,
  wl_seat_e_capabilities_op = 0,
  wl_seat_e_name_op,
  // wl_keyboard
  wl_keyboard_e_keymap_op = 0,
  wl_keyboard_e_enter_op,
  wl_keyboard_e_leave_op,
  wl_keyboard_e_key_op,
  wl_keyboard_e_modifiers_op,
  wl_keyboard_e_repeat_info_op,
  // wl_shm
  wl_shm_r_create_pool_op = 0,
  wl_shm_e_format_op = 0,
  // wl_shm_pool
  wl_shm_pool_r_create_buffer_op = 0,
  // wl_buffer
  wl_buffer_e_release_op = 0,
};

// protocols

// xx utils
const wlw_msg_view *wlw_recv_until_sync (wl_callback *callback);
const wlw_msg_view *wlw_recv_for_opcode (wlw_interface interface,
                                         wlw_opcode opcode);

// wl_display
wl_registry wl_display_r_get_registry (wlw_static_new_id registry);
wl_callback wl_display_r_sync (wlw_static_new_id callback);
void wl_display_e_delete_id (const wlw_msg_view *msg);

// wl_callback
wlw_uint wl_callback_e_done (const wlw_msg_view *msg);

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
void wl_surface_r_attach (wl_surface self, wl_buffer buffer, wlw_uint x,
                          wlw_uint y);
wlw_uint wl_surface_e_preferred_buffer_scale (const wlw_msg_view *msg);
wlw_uint wl_surface_e_preferred_buffer_transform (const wlw_msg_view *msg);

// xdg_wm_base
xdg_surface xdg_wm_base_r_get_xdg_surface (xdg_wm_base self,
                                           wlw_static_new_id id,
                                           wl_surface surface);
typedef struct
{
  wlw_uint serial;
} xdg_wm_base_e_ping_serial;
xdg_wm_base_e_ping_serial xdg_wm_base_e_ping (const wlw_msg_view *msg);
void xdg_wm_base_r_pong (xdg_wm_base self, xdg_wm_base_e_ping_serial serial);

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

void xdg_toplevel_e_close (const wlw_msg_view *msg);

// wl_seat
wl_keyboard wl_seat_r_get_keyboard (wl_seat self, wlw_static_new_id id);
typedef enum
{
  wl_seat_capability_pointer = 1 << 0,
  wl_seat_capability_keyboard = 1 << 1,
  wl_seat_capability_touch = 1 << 2,
} wl_seat_capability;
wl_seat_capability wl_seat_e_capabilities (const wlw_msg_view *msg);
wlw_string *wl_seat_e_name (const wlw_msg_view *msg);

// wl_keyboard
void wl_keyboard_e_keymap (const wlw_msg_view *msg);
void wl_keyboard_e_repeat_info (const wlw_msg_view *msg);
typedef struct
{
  wlw_uint serial;
  wl_surface surface;
  wlw_array keys;
} wl_keyboard_e_enter_args;
const wl_keyboard_e_enter_args *wl_keyboard_e_enter (const wlw_msg_view *msg);
typedef struct
{
  wlw_uint serial;
  wlw_uint mods_depressed;
  wlw_uint mods_latched;
  wlw_uint mods_locked;
  wlw_uint group;
} wl_keyboard_e_modifiers_args;
const wl_keyboard_e_modifiers_args *wl_keyboard_e_modifiers (const
                                                             wlw_msg_view
                                                             *msg);
typedef struct
{
  wlw_uint serial;
  wlw_uint time;
  wlw_uint key;
  union
  {
    wlw_uint as_raw;
    enum
    {
      wl_keyboard_key_state_released = 0,
      wl_keyboard_key_state_pressed,
      wl_keyboard_key_state_repeated,
    } as_enum;
  } state;
} wl_keyboard_e_key_args;
_Static_assert (sizeof (((wl_keyboard_e_key_args *)0)->state) ==
                sizeof (wlw_uint));
const wl_keyboard_e_key_args *wl_keyboard_e_key (const wlw_msg_view *msg);

static const char wlw_evdev_to_ascii[] = {
  // alphabet
  [KEY_A] = 'A',[KEY_B] = 'B',[KEY_C] = 'C',[KEY_D] = 'D',[KEY_E] = 'E',
  [KEY_F] = 'F',[KEY_G] = 'G',[KEY_H] = 'H',[KEY_I] = 'I',[KEY_J] = 'J',
  [KEY_K] = 'K',[KEY_L] = 'L',[KEY_M] = 'M',[KEY_N] = 'N',[KEY_O] = 'O',
  [KEY_P] = 'P',[KEY_Q] = 'Q',[KEY_R] = 'R',[KEY_S] = 'S',[KEY_T] = 'T',
  [KEY_U] = 'U',[KEY_V] = 'V',[KEY_W] = 'W',[KEY_X] = 'X',[KEY_Y] = 'Y',
  [KEY_Z] = 'Z',
  // numbers
  [KEY_1] = '1',[KEY_2] = '2',[KEY_3] = '3',[KEY_4] = '4',[KEY_5] = '5',
  [KEY_6] = '6',[KEY_7] = '7',[KEY_8] = '8',[KEY_9] = '9',[KEY_0] = '0',
};

// wl_shm
wl_shm_pool wl_shm_r_create_pool (wl_shm self, wlw_static_new_id id,
                                  int fd, wlw_uint size);
typedef enum
{
  wl_shm_format_argb8888 = 0,
  wl_shm_format_xrgb8888 = 1,
} wl_shm_format;
wl_shm_format wl_shm_e_format (const wlw_msg_view *msg);

// wl_shm_pool
wl_buffer
wl_shm_pool_r_create_buffer (wl_shm_pool self, wlw_static_new_id id,
                             wlw_uint offset, wlw_uint width,
                             wlw_uint height, wlw_uint stride,
                             wl_shm_format format);

// wl_buffer
void wl_buffer_e_release (const wlw_msg_view *msg);

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

void
wlw_send_with_fd (const wlw_msg_view *msg, int fd)
{
  struct msghdr prep = { 0 };
  wlw_size size = wlw_msg_size (msg);

  struct iovec iov = {.iov_base = (void *) msg,.iov_len = size };
  prep.msg_iov = &iov;
  prep.msg_iovlen = 1;

  wlw_byte buf[CMSG_SPACE (sizeof (fd))] = { 0 };
  prep.msg_control = buf;
  prep.msg_controllen = sizeof (buf);
  struct cmsghdr *cmsg = CMSG_FIRSTHDR (&prep);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN (sizeof (fd));
  *((int *) CMSG_DATA (cmsg)) = fd;

  int n = sendmsg (wlw_sockfd, &prep, 0);
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
wlw_peek ()
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

  return msg;
}

static inline const wlw_msg_view *
wlw_recv ()
{
  const wlw_msg_view *msg = wlw_peek ();
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

static inline wlw_opcode
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
wlw_size_opcode (wlw_size size, wlw_opcode opcode)
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
  wlw_assert (new_id.repr <
              (sizeof (wlw_obj_map) / sizeof (wlw_obj_map[0])),
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
      wl_callback_e_done (msg);
      wlw_assert (is_callback_pending, "unexpected callback completion");
      is_callback_pending = false;

      wl_display_e_delete_id (wlw_recv ());
      callback->as_obj = wlw_object_invalid;
      return NULL;
    }
  return msg;
}

const wlw_msg_view *
wlw_recv_for_opcode (wlw_interface interface, wlw_opcode opcode)
{
  const wlw_msg_view *msg = wlw_peek ();
  if (wlw_obj_typeof (msg->hdr.object) == interface &&
      wlw_msg_opcode (msg) == opcode)
    {
      wlw_reader_next += wlw_msg_size (msg);
      return msg;
    }
  return NULL;
}

const wlw_msg_view *
wlw_recv_until_opcode (wlw_interface interface, wlw_opcode opcode)
{
  const wlw_msg_view *msg = wlw_peek ();
  if (wlw_obj_typeof (msg->hdr.object) == interface &&
      wlw_msg_opcode (msg) == opcode)
    {
      return NULL;
    }
  wlw_reader_next += wlw_msg_size (msg);
  return msg;
}

// wl_display
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
    wlw_size_opcode (sizeof (msg), wl_display_r_get_registry_op);
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
  msg.hdr.size_opcode = wlw_size_opcode (sizeof (msg), wl_display_r_sync_op);
  wlw_send ((wlw_msg_view *) &msg);

  wl_callback ret = { wlw_obj_bind (wl_callback_i, callback) };
  return ret;
}

void
wl_display_e_delete_id (const wlw_msg_view *msg)
{
  wlw_assert (msg->hdr.object.id == 1);
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_display_i);

  wlw_object obj = { msg->payload[0] };
  wlw_obj_unbind (obj);
}

// wl_callback
wlw_uint
wl_callback_e_done (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_callback_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_callback_e_done_op);

  return msg->payload[0];
}

// wl_registry
wl_registry_e_global_args
wl_registry_e_global (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_registry_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_registry_e_global_op);

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
  msg->hdr.size_opcode = wlw_size_opcode (size, wl_registry_r_bind_op);
  wlw_send (msg);
  return wlw_obj_bind (interface, id);
}

// wl_compositor

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
    wlw_size_opcode (sizeof (msg), wl_compositor_r_create_surface_op);
  wlw_send ((wlw_msg_view *) &msg);

  wl_surface ret = { wlw_obj_bind (wl_surface_i, id) };
  return ret;
}

// wl_surface
void
wl_surface_r_commit (wl_surface self)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_surface_i);

  struct
  {
    wlw_header hdr;
  } msg;

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), wl_surface_r_commit_op);
  wlw_send ((wlw_msg_view *) &msg);
}

void
wl_surface_r_attach (wl_surface self, wl_buffer buffer, wlw_uint x,
                     wlw_uint y)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_surface_i);
  wlw_assert (wlw_obj_typeof (buffer.as_obj) == wl_buffer_i);

  struct
  {
    wlw_header hdr;
    wl_buffer buffer;
    wlw_uint x;
    wlw_uint y;
  } msg = {
    .buffer = buffer,
    .x = x,
    .y = y,
  };

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), wl_surface_r_attach_op);
  wlw_send ((wlw_msg_view *) &msg);
}

wlw_uint
wl_surface_e_preferred_buffer_scale (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_surface_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_surface_e_preferred_buffer_scale_op);
  return msg->payload[0];
}

wlw_uint
wl_surface_e_preferred_buffer_transform (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_surface_i);
  wlw_assert (wlw_msg_opcode (msg) ==
              wl_surface_e_preferred_buffer_transform_op);
  return msg->payload[0];
}

// xdg_wm_base
xdg_surface
xdg_wm_base_r_get_xdg_surface (xdg_wm_base self,
                               wlw_static_new_id id, wl_surface surface)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == xdg_wm_base_i);
  wlw_assert (wlw_obj_typeof (surface.as_obj) == wl_surface_i);
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
    wlw_size_opcode (sizeof (msg), xdg_wm_base_r_get_xdg_surface_op);
  wlw_send ((wlw_msg_view *) &msg);

  xdg_surface ret = { wlw_obj_bind (xdg_surface_i, id) };
  return ret;
}

xdg_wm_base_e_ping_serial
xdg_wm_base_e_ping (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_wm_base_i);
  wlw_assert (wlw_msg_opcode (msg) == xdg_wm_base_e_ping_op);
  xdg_wm_base_e_ping_serial ret = { msg->payload[0] };
  return ret;
}

void
xdg_wm_base_r_pong (xdg_wm_base self, xdg_wm_base_e_ping_serial serial)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == xdg_wm_base_i);
  struct
  {
    wlw_header hdr;
    xdg_wm_base_e_ping_serial serial;
  } msg = {
    .serial = serial,
  };
  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode = wlw_size_opcode (sizeof (msg), xdg_wm_base_r_pong_op);
  wlw_send ((wlw_msg_view *) &msg);
}

// xdg_surface
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
    wlw_size_opcode (sizeof (msg), xdg_surface_r_get_toplevel_op);
  wlw_send ((wlw_msg_view *) &msg);

  xdg_toplevel ret = { wlw_obj_bind (xdg_toplevel_i, id) };
  return ret;
}

xdg_surface_configure_serial
xdg_surface_e_configure (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_surface_i);
  wlw_assert (wlw_msg_opcode (msg) == xdg_surface_e_configure_op);

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
    wlw_size_opcode (sizeof (msg), xdg_surface_r_ack_configure_op);
  wlw_send ((wlw_msg_view *) &msg);
}

// xdg_toplevel
const xdg_toplevel_e_configure_args *
xdg_toplevel_e_configure (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_toplevel_i);
  wlw_assert (wlw_msg_opcode (msg) == xdg_toplevel_e_configure_op);

  // trailing dynamic member, castable to flexible member
  return (xdg_toplevel_e_configure_args *) msg->payload;
}

void
xdg_toplevel_e_wm_capabilities (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_toplevel_i);
  wlw_assert (wlw_msg_opcode (msg) == xdg_toplevel_e_wm_capabilities_op);
}

void
xdg_toplevel_e_close (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == xdg_toplevel_i);
  wlw_assert (wlw_msg_opcode (msg) == xdg_toplevel_e_close_op);
}

// wl_seat
wl_seat_capability
wl_seat_e_capabilities (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_seat_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_seat_e_capabilities_op);

  return msg->payload[0];
}

wlw_string *
wl_seat_e_name (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_seat_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_seat_e_name_op);

  return (wlw_string *) msg->payload;
}

wl_keyboard
wl_seat_r_get_keyboard (wl_seat self, wlw_static_new_id id)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_seat_i);

  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
  } msg;
  msg.id = id;
  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), wl_seat_r_get_keyboard_op);
  wlw_send ((wlw_msg_view *) &msg);
  wl_keyboard ret = { wlw_obj_bind (wl_keyboard_i, id) };
  return ret;
}

// wl_keyboard

// just consume for now, need to redo buffering for fds, which chances are i
// may never need. look into it when i need it
void
wl_keyboard_e_keymap (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_keyboard_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_keyboard_e_keymap_op);
  // since we read it via wlw_read, the kernel probably closes it
  // automatically, dont quote me
}

void
wl_keyboard_e_repeat_info (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_keyboard_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_keyboard_e_repeat_info_op);
  // since we read it via wlw_read, the kernel probably closes it
  // automatically, dont quote me
}

const wl_keyboard_e_enter_args *
wl_keyboard_e_enter (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_keyboard_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_keyboard_e_enter_op);
  return (const wl_keyboard_e_enter_args *) msg->payload;
}

const wl_keyboard_e_modifiers_args *
wl_keyboard_e_modifiers (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_keyboard_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_keyboard_e_modifiers_op);
  return (const wl_keyboard_e_modifiers_args *) msg->payload;
}

const wl_keyboard_e_key_args *
wl_keyboard_e_key (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_keyboard_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_keyboard_e_key_op);
  return (const wl_keyboard_e_key_args *) msg->payload;
}

// wl_shm
wl_shm_pool
wl_shm_r_create_pool (wl_shm self, wlw_static_new_id id, int fd,
                      wlw_uint size)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_shm_i);
  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
    wlw_uint size;
  } msg = {
    .id = id,
    .size = size,
  };

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), wl_shm_r_create_pool_op);

  wlw_send_with_fd ((wlw_msg_view *) &msg, fd);
  wl_shm_pool ret = { wlw_obj_bind (wl_shm_pool_i, id) };
  return ret;
}

wl_shm_format
wl_shm_e_format (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_shm_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_shm_e_format_op);

  return msg->payload[0];
}

// wl_shm_pool
wl_buffer
wl_shm_pool_r_create_buffer (wl_shm_pool self, wlw_static_new_id id,
                             wlw_uint offset, wlw_uint width,
                             wlw_uint height, wlw_uint stride,
                             wl_shm_format format)
{
  wlw_assert (wlw_obj_typeof (self.as_obj) == wl_shm_pool_i);

  struct
  {
    wlw_header hdr;
    wlw_static_new_id id;
    wlw_uint offset;
    wlw_uint width;
    wlw_uint height;
    wlw_uint stride;
    wlw_uint format;
  } msg = {
    .id = id,
    .offset = offset,
    .width = width,
    .height = height,
    .stride = stride,
    .format = format,
  };

  msg.hdr.object = self.as_obj;
  msg.hdr.size_opcode =
    wlw_size_opcode (sizeof (msg), wl_shm_pool_r_create_buffer_op);
  wlw_send ((wlw_msg_view *) &msg);
  wl_buffer ret = { wlw_obj_bind (wl_buffer_i, id) };
  return ret;
}

// wl_buffer
void
wl_buffer_e_release (const wlw_msg_view *msg)
{
  wlw_assert (wlw_obj_typeof (msg->hdr.object) == wl_buffer_i);
  wlw_assert (wlw_msg_opcode (msg) == wl_buffer_e_release_op);
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
  wl_seat seat = { wlw_object_invalid };
  xdg_wm_base wm_base = { wlw_object_invalid };
  wl_shm shm = { wlw_object_invalid };

  const wlw_msg_view *msg;
  while ((msg = wlw_recv_for_opcode (wl_registry_i, wl_registry_e_global_op)))
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
      else if (strcmp (iname, wlw_interface_names[wl_seat_i].str) == 0)
        {
          seat.as_obj =
            wl_registry_r_bind (registry, args.name, wl_seat_i,
                                args.version, wlw_obj_genid ());
        }
      else if (strcmp (iname, wlw_interface_names[wl_shm_i].str) == 0)
        {
          shm.as_obj = wl_registry_r_bind (registry, args.name, wl_shm_i,
                                           args.version, wlw_obj_genid ());
        }
      // else printf ("%s\n", iname);
    }

  wl_surface surface =
    wl_compositor_r_create_surface (compositor, wlw_obj_genid ());
  xdg_surface shell_surface =
    xdg_wm_base_r_get_xdg_surface (wm_base, wlw_obj_genid (), surface);
  xdg_toplevel toplevel =
    xdg_surface_r_get_toplevel (shell_surface, wlw_obj_genid ());
  (void) toplevel;
  wl_surface_r_commit (surface);

  wl_seat_e_name (wlw_recv ());
  wl_seat_capability seat_caps = wl_seat_e_capabilities (wlw_recv ());
  wlw_assert (seat_caps & wl_seat_capability_keyboard, "no keyboard found");

  bool xrgb_supported = false;
  while ((msg = wlw_recv_for_opcode (wl_shm_i, wl_shm_e_format_op)))
    xrgb_supported = xrgb_supported
      || (wl_shm_e_format (msg) == wl_shm_format_xrgb8888);
  wlw_assert (xrgb_supported);

  xdg_toplevel_e_wm_capabilities (wlw_recv ());
  const xdg_toplevel_e_configure_args *args =
    xdg_toplevel_e_configure (wlw_recv ());

  xdg_surface_r_ack_configure (shell_surface,
                               xdg_surface_e_configure (wlw_recv ()));

  wl_keyboard keyboard = wl_seat_r_get_keyboard (seat, wlw_obj_genid ());
  (void) keyboard;
  wl_keyboard_e_keymap (wlw_recv ());
  wl_keyboard_e_repeat_info (wlw_recv ());

  int fd = memfd_create ("wl_shm", MFD_CLOEXEC);
  int size = args->width * args->height * 4;
  ftruncate (fd, size);
  wlw_word *fb = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  (void) fb;
  wl_shm_pool pool = wl_shm_r_create_pool (shm, wlw_obj_genid (), fd, size);
  wl_buffer buf =
    wl_shm_pool_r_create_buffer (pool, wlw_obj_genid (), 0, args->width,
                                 args->height, args->width * 4,
                                 wl_shm_format_xrgb8888);
  for (wlw_uint y = 0; y < args->height; y++)
    for (wlw_uint x = 0; x < args->width; x++)
      fb[y * args->width + x] = 0xFF000000
        | (((wlw_uint) (0xFF * (y * 1.f / args->height))) << 8)
        | (((wlw_uint) (0xFF * (x * 1.f / args->width))) << 0);
  wl_surface_r_attach (surface, buf, 0, 0);
  wl_surface_r_commit (surface);

  while ((msg =
          wlw_recv_until_opcode (xdg_toplevel_i, xdg_toplevel_e_close_op)))
    {
      switch (wlw_obj_typeof (msg->hdr.object))
        {
        case xdg_toplevel_i:
          wlw_assert (wlw_msg_opcode (msg) == xdg_toplevel_e_configure_op);
          args = xdg_toplevel_e_configure (msg);
          break;
        case wl_surface_i:
          switch (wlw_msg_opcode (msg))
            {
            case wl_surface_e_preferred_buffer_scale_op:
              wl_surface_e_preferred_buffer_scale (msg);
              break;
            case wl_surface_e_preferred_buffer_transform_op:
              wl_surface_e_preferred_buffer_transform (msg);
              break;
            }
          break;
        case wl_keyboard_i:
          switch (wlw_msg_opcode (msg))
            {
            case wl_keyboard_e_enter_op:
              {
                const wl_keyboard_e_enter_args *enter_args =
                  wl_keyboard_e_enter (msg);
                wlw_assert (enter_args->surface.as_obj.id ==
                            surface.as_obj.id);
              } break;
            case wl_keyboard_e_modifiers_op:
              wl_keyboard_e_modifiers (msg);
              break;
            case wl_keyboard_e_key_op:
              {
                //wlw_print_msg (msg);
                const wl_keyboard_e_key_args *ev = wl_keyboard_e_key (msg);
                if (ev->state.as_enum == wl_keyboard_key_state_released)
                  printf ("pressed key %c\n", wlw_evdev_to_ascii[ev->key]);
              }
              break;
            default:
              // wlw_print_msg (msg);
              break;
            }
          break;
        case wl_buffer_i:
          wlw_assert (wlw_msg_opcode (msg) == wl_buffer_e_release_op);
          wl_buffer_e_release (msg);
          break;
        case xdg_surface_i:
          wlw_assert (wlw_msg_opcode (msg) == xdg_surface_e_configure_op);
          xdg_surface_r_ack_configure (shell_surface,
                                       xdg_surface_e_configure (msg));
          break;
        case xdg_wm_base_i:
          wlw_assert (wlw_msg_opcode (msg) == xdg_wm_base_e_ping_op);
          xdg_wm_base_r_pong (wm_base, xdg_wm_base_e_ping (msg));
          break;
        default:
          wlw_print_msg (msg);
          break;
        }
    }

  xdg_toplevel_e_close (wlw_recv ());

  printf ("todo list:\n");
  wl_callback syncpoint = { wlw_object_invalid };
  while ((msg = wlw_recv_until_sync (&syncpoint)))
    wlw_print_msg (msg);
}

#endif
