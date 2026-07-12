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
  struct sockaddr_un addr;
  wlw_sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
  addr.sun_family = AF_UNIX;
  snprintf (addr.sun_path, sizeof (addr.sun_path), "%s/%s",
            getenv ("XDG_RUNTIME_DIR"), getenv ("WAYLAND_DISPLAY"));
  connect (wlw_sockfd, (const struct sockaddr *) &addr, sizeof (addr));
}

typedef uint32_t wlw_word;

struct wlw_untyped
{
  wlw_word object;
  wlw_word size_opcode;         /* endian-aware */
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
  /* check if we atleast have a size: [object][size||opcode][...] */
  if (reader_end - reader_head < (int) sizeof (wlw_word) * 2)
    _wlw_recv_refill ();
  msg = (const struct wlw_untyped *) &reader_buf[reader_head];
  if (reader_end - reader_head < (int) msg->size_opcode >> 16)
    _wlw_recv_refill ();
  msg = (const struct wlw_untyped *) &reader_buf[reader_head];

  reader_head += msg->size_opcode >> 16;
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
  wl_compositor_e_len = 0
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
  wl_shm_pool_e_len = 0
};
enum wl_buffer_interface
{
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
  { sizeof ("wl_display_i"), "wl_display_i" },
  { sizeof ("wl_callback_i"), "wl_callback_i" },
  { sizeof ("wl_registry_i"), "wl_registry_i" },
  { sizeof ("wl_compositor_i"), "wl_compositor_i" },
  { sizeof ("wl_surface_i"), "wl_surface_i" },
  { sizeof ("xdg_wm_base_i"), "xdg_wm_base_i" },
  { sizeof ("xdg_surface_i"), "xdg_surface_i" },
  { sizeof ("xdg_toplevel_i"), "xdg_toplevel_i" },
  { sizeof ("wl_seat_i"), "wl_seat_i" },
  { sizeof ("wl_keyboard_i"), "wl_keyboard_i" },
  { sizeof ("wl_shm_i"), "wl_shm_i" },
  { sizeof ("wl_shm_pool_i"), "wl_shm_pool_i" },
  { sizeof ("wl_buffer_i"), "wl_buffer_i" },
};

typedef void (*wlw_callback) (const struct wlw_untyped * msg);
struct _wlw_dispatch_table
{
  wlw_callback wl_display[wl_display_e_len];
  wlw_callback wl_callback[wl_callback_e_len];
  wlw_callback wl_registry[wl_registry_e_len];
  /* wlw_callback wl_compositor[wl_compositor_e_len]; */
  wlw_callback wl_surface[wl_surface_e_len];
  wlw_callback xdg_wm_base[xdg_wm_base_e_len];
  wlw_callback xdg_surface[xdg_surface_e_len];
  wlw_callback xdg_toplevel[xdg_toplevel_e_len];
  wlw_callback wl_seat[wl_seat_e_len];
  wlw_callback wl_keyboard[wl_keyboard_e_len];
  wlw_callback wl_shm[wl_shm_e_len];
  /* wlw_callback wl_shm_pool[wl_shm_pool_e_len]; */
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
  /* offsetof (struct _wlw_dispatch_table, wl_compositor) / sizeof (wlw_callback), */
  offsetof (struct _wlw_dispatch_table, wl_surface) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_wm_base) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_surface) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, xdg_toplevel) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_seat) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_keyboard) / sizeof (wlw_callback),
  offsetof (struct _wlw_dispatch_table, wl_shm) / sizeof (wlw_callback),
  /* offsetof (struct _wlw_dispatch_table, wl_shm_pool) / sizeof (wlw_callback), */
  offsetof (struct _wlw_dispatch_table, wl_buffer) / sizeof (wlw_callback),
};

static void
wlw_register (enum wlw_interface interface, int opcode, wlw_callback cb)
{
  int entry = wlw_dispatch_table_offsets[interface] + opcode;
  wlw_dispatch_table.as_array[entry] = cb;
}

#endif /* _WLW_C */
