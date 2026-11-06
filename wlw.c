#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#ifndef _WLW_C
#define _WLW_C

static int wlw_sockfd = -1;

static void
wlw_open (void)
{
  const char *runtime_dir, *wayland_display;
  struct sockaddr_un addr;

  addr.sun_family = AF_UNIX;
  runtime_dir = getenv ("XDG_RUNTIME_DIR");
  wayland_display = getenv ("WAYLAND_DISPLAY");

  if (runtime_dir == NULL || *runtime_dir == '\0')
    runtime_dir = "/run/user/1000";
  if (wayland_display == NULL || *wayland_display == '\0')
    wayland_display = "wayland-0";

  assert (strlen (runtime_dir) + 1 + strlen (wayland_display) + 1 <=
          sizeof (addr.sun_path) && "socket path is too long");

  strcpy (addr.sun_path, runtime_dir);
  strcat (addr.sun_path, "/");
  strcat (addr.sun_path, wayland_display);

  wlw_sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  connect (wlw_sockfd, (const struct sockaddr *) &addr, sizeof (addr));
}

typedef uint32_t wlw_word;
#define wlw_bytes_to_words(bytes) ((bytes + 3) / 4)

struct wlw_string
{
  wlw_word len;
  char str[1];
};

struct wlw_header
{
  wlw_word object;
  wlw_word size_opcode;         /* endian-aware */
};

static struct wlw_header
wlw_make_header (wlw_word object, uint16_t size, uint16_t opcode)
{
  struct wlw_header hdr;
  hdr.object = object;
  hdr.size_opcode = ((uint32_t) size << 16) | opcode;
  return hdr;
}

static uint16_t
wlw_get_size (const struct wlw_header *hdr)
{
  return hdr->size_opcode >> 16;
}

static uint16_t
wlw_get_opcode (const struct wlw_header *hdr)
{
  return hdr->size_opcode & ((1 << 16) - 1);
}


struct wlw_untyped
{
  struct wlw_header hdr;
  wlw_word payload[1];
};

static char reader_buf[1024];
static int reader_head = 0, reader_end = 0;

static void
_wlw_recv_refill ()
{
  /* compact */
  memmove (reader_buf, &reader_buf[reader_head], reader_end - reader_head);
  reader_end -= reader_head, reader_head = 0;

  reader_end +=
    read (wlw_sockfd, &reader_buf[reader_end],
          sizeof (reader_buf) - reader_end);
}

static const struct wlw_untyped *
wlw_recv (void)
{
  const struct wlw_untyped *msg;
  /* check if we atleast have a size: [object][size|opcode][...] */
  if (reader_end - reader_head < (int) sizeof (struct wlw_header))
    _wlw_recv_refill ();
  msg = (const struct wlw_untyped *) &reader_buf[reader_head];
  if (reader_end - reader_head < wlw_get_size (&msg->hdr))
    _wlw_recv_refill ();
  msg = (const struct wlw_untyped *) &reader_buf[reader_head];

  reader_head += wlw_get_size (&msg->hdr);
  return msg;
}

enum wl_display_interface
{
  wl_display_r_sync_op = 0,
  wl_display_r_get_registry_op,

  wl_display_e_error_op = 0,
  wl_display_e_delete_id_op,
  wl_display_e_len
};
enum wl_callback_interface
{
  wl_callback_e_done_op = 0,
  wl_callback_e_len
};
enum wl_registry_interface
{
  wl_registry_r_bind_op = 0,

  wl_registry_e_global_op = 0,
  wl_registry_e_len
};
enum wl_compositor_interface
{
  wl_compositor_r_create_surface_op = 0,
  wl_compositor_e_len = 1
};
enum wl_surface_interface
{
  wl_surface_r_destroy_op = 0,
  wl_surface_r_attach_op,
  wl_surface_r_damage_op,       /* looks deprecated */
  wl_surface_r_frame_op,
  wl_surface_r_set_opaque_region_op,
  wl_surface_r_set_input_region_op,
  wl_surface_r_commit_op,
  wl_surface_r_set_buffer_transform_op,
  wl_surface_r_set_buffer_scale_op,
  wl_surface_r_damage_buffer_op,

  wl_surface_e_enter_op = 0,
  wl_surface_e_leave_op,
  wl_surface_e_preferred_buffer_scale_op,
  wl_surface_e_preferred_buffer_transform_op,
  wl_surface_e_len
};
enum xdg_wm_base_interface
{
  xdg_wm_base_r_destroy_op = 0,
  xdg_wm_base_r_create_positioner_op,
  xdg_wm_base_r_get_xdg_surface_op,
  xdg_wm_base_r_pong_op,

  xdg_wm_base_e_ping_op = 0,
  xdg_wm_base_e_len
};
enum xdg_surface_interface
{
  xdg_surface_r_destroy_op = 0,
  xdg_surface_r_get_toplevel_op,
  xdg_surface_r_get_popup_op,
  xdg_surface_r_set_window_geometry_op,
  xdg_surface_r_ack_configure_op,

  xdg_surface_e_configure_op = 0,
  xdg_surface_e_len
};
enum xdg_toplevel_interface
{
  xdg_toplevel_e_configure_op = 0,
  xdg_toplevel_e_close_op,
  xdg_toplevel_e_configure_bounds_op,
  xdg_toplevel_e_wm_capabilities_op,
  xdg_toplevel_e_len
};
enum wl_seat_interface
{
  wl_seat_r_get_pointer_op = 0,
  wl_seat_r_get_keyboard_op,

  wl_seat_e_capabilities_op = 0,
  wl_seat_e_name_op,
  wl_seat_e_len
};
enum wl_keyboard_interface
{
  wl_keyboard_e_keymap_op = 0,
  wl_keyboard_e_enter_op,
  wl_keyboard_e_leave_op,
  wl_keyboard_e_key_op,
  wl_keyboard_e_modifiers_op,
  wl_keyboard_e_repeat_info_op,
  wl_keyboard_e_len
};
enum wl_shm_interface
{
  wl_shm_r_create_pool_op = 0,
  wl_shm_e_format_op = 0,
  wl_shm_e_len
};
enum wl_shm_pool_interface
{
  wl_shm_pool_r_create_buffer_op = 0,
  wl_shm_pool_r_destroy_op,
  wl_shm_pool_r_resize_op,
  wl_shm_pool_e_len = 1
};
enum wl_buffer_interface
{
  wl_buffer_r_destroy_op = 0,
  wl_buffer_e_release_op = 0,
  wl_buffer_e_len
};

enum wlw_interface
{
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
  wlw_interfaces_len
};

static const struct
{
  const wlw_word len;
  const char str[16];
} wlw_interface_names[wlw_interfaces_len] = {
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

typedef void (*wlw_callback) (const struct wlw_untyped * msg);
struct _wlw_dispatch_table
{
  wlw_callback wl_display[wl_display_e_len];
  wlw_callback wl_callback[wl_callback_e_len];
  wlw_callback wl_registry[wl_registry_e_len];
  wlw_callback wl_compositor[wl_compositor_e_len];
  wlw_callback wl_surface[wl_surface_e_len];
  wlw_callback xdg_wm_base[xdg_wm_base_e_len];
  wlw_callback xdg_surface[xdg_surface_e_len];
  wlw_callback xdg_toplevel[xdg_toplevel_e_len];
  wlw_callback wl_seat[wl_seat_e_len];
  wlw_callback wl_keyboard[wl_keyboard_e_len];
  wlw_callback wl_shm[wl_shm_e_len];
  wlw_callback wl_shm_pool[wl_shm_pool_e_len];
  wlw_callback wl_buffer[wl_buffer_e_len];
};

static union
{
  struct _wlw_dispatch_table as_struct;
  wlw_callback as_array[1];
} wlw_dispatch_table;

static const int wlw_dispatch_table_offsets[wlw_interfaces_len] = {
  offsetof (struct _wlw_dispatch_table, wl_display) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_callback) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_registry) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table,
            wl_compositor) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_surface) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_wm_base) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_surface) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_toplevel) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_seat) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_keyboard) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_shm) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_shm_pool) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_buffer) / sizeof (wlw_callback),
};

static void
wlw_register (enum wlw_interface interface, int opcode, wlw_callback cb)
{
  int entry = wlw_dispatch_table_offsets[interface] + opcode;
  wlw_dispatch_table.as_array[entry] = cb;
}

typedef wlw_word wlw_new_id;

enum wlw_interface wlw_typeof[64];

static wlw_new_id
wlw_genid (enum wlw_interface interface)
{
  static wlw_word ctr = 1;
  ctr++;
  wlw_typeof[ctr] = interface;
  return ctr;
}

static void
wlw_dispatch (const struct wlw_untyped *msg)
{
  enum wlw_interface interface = wlw_typeof[msg->hdr.object];
  uint16_t opcode = wlw_get_opcode (&msg->hdr);
  int entry = wlw_dispatch_table_offsets[interface] + opcode;
  wlw_callback cb = wlw_dispatch_table.as_array[entry];
  if (cb)
    cb (msg);
}

struct wl_display_r_get_registry
{
  struct wlw_header hdr;
  wlw_new_id registry;
};

struct wl_display_e_error
{
  struct wlw_header hdr;
  wlw_word object_id;
  wlw_word code;
  struct wlw_string message;
};

struct wl_registry_e_global_ptrs
{
  const wlw_word *name;
  const struct wlw_string *interface;
  const wlw_word *version;
};

static struct wl_registry_e_global_ptrs
wl_registry_e_global (const struct wlw_untyped *msg)
{
  struct wl_registry_e_global_ptrs ptrs;
  const wlw_word *reader = msg->payload;
  ptrs.name = reader++;
  ptrs.interface = (void *) reader;
  reader += 1 + wlw_bytes_to_words (ptrs.interface->len);
  ptrs.version = reader++;
  return ptrs;
}

static wlw_new_id
wl_registry_r_bind (wlw_word registry, enum wlw_interface interface,
                    struct wl_registry_e_global_ptrs ptrs)
{
  wlw_word id = wlw_genid (interface);
  struct
  {
    struct wlw_header hdr;
    wlw_word name;
    wlw_word iname_len;
    char iname_str[16];
    wlw_word version;
    wlw_new_id id;
  } max_msg;
  wlw_word *reader = (void *) &max_msg;

  reader += wlw_bytes_to_words (sizeof (struct wlw_header));
  *(reader++) = *ptrs.name;
  *(reader++) = wlw_interface_names[interface].len;
  memcpy (reader, wlw_interface_names[interface].str,
          wlw_interface_names[interface].len);
  reader += wlw_bytes_to_words (wlw_interface_names[interface].len);
  *(reader++) = *ptrs.version;
  *(reader++) = id;
  max_msg.hdr =
    wlw_make_header (registry, (char *) reader - (char *) &max_msg,
                     wl_registry_r_bind_op);
  write (wlw_sockfd, &max_msg, wlw_get_size (&max_msg.hdr));
  return id;
}

struct wl_compositor_r_create_surface
{
  struct wlw_header hdr;
  wlw_new_id id;
};

struct xdg_wm_base_r_get_xdg_surface
{
  struct wlw_header hdr;
  wlw_new_id id;
  wlw_word surface;
};

struct xdg_wm_base_e_ping
{
  wlw_word serial;
};

struct xdg_wm_base_r_pong
{
  struct wlw_header hdr;
  wlw_word serial;
};

struct xdg_surface_r_get_toplevel
{
  struct wlw_header hdr;
  wlw_new_id id;
};

struct wl_surface_r_commit
{
  struct wlw_header hdr;
};

struct xdg_surface_e_configure
{
  wlw_word serial;
};

struct xdg_toplevel_e_configure
{
  wlw_word width;
  wlw_word height;
  wlw_word states_len;
  wlw_word states_arr[1];
};

struct xdg_surface_r_ack_configure
{
  struct wlw_header hdr;
  wlw_word serial;
};

struct wl_shm_e_format
{
  wlw_word format;
};

enum wl_shm_format
{
  wl_shm_format_argb8888 = 0,
  wl_shm_format_xrgb8888 = 1
};

struct wl_shm_r_create_pool
{
  struct wlw_header hdr;
  wlw_new_id id;
  /* fd shm_fd; */
  wlw_word size;
};

struct wl_shm_pool_r_resize
{
  struct wlw_header hdr;
  wlw_word size;
};

struct wl_shm_pool_r_create_buffer
{
  struct wlw_header hdr;
  wlw_new_id id;
  wlw_word offset;
  wlw_word width;
  wlw_word height;
  wlw_word stride;
  wlw_word format;
};

struct wl_surface_r_attach
{
  struct wlw_header hdr;
  wlw_word buffer;
  wlw_word x;
  wlw_word y;
};

struct wl_surface_r_damage_buffer
{
  struct wlw_header hdr;
  wlw_word x;
  wlw_word y;
  wlw_word width;
  wlw_word height;
};


struct wl_buffer_r_destroy
{
  struct wlw_header hdr;
};

#endif /* _WLW_C */
