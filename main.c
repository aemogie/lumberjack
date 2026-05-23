#include "wlw.c"
#include <string.h>

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

  bool argb_supported = false;
  while ((msg = wlw_recv_for_opcode (wl_shm_i, wl_shm_e_format_op)))
    argb_supported = argb_supported
      || (wl_shm_e_format (msg) == wl_shm_format_argb8888);
  wlw_assert (argb_supported);

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
  wl_shm_pool pool = wl_shm_r_create_pool (shm, wlw_obj_genid (), fd, size);
  wl_buffer buf =
    wl_shm_pool_r_create_buffer (pool, wlw_obj_genid (), 0, args->width,
                                 args->height, args->width * 4,
                                 wl_shm_format_argb8888);
  for (wlw_uint y = 0; y < args->height; y++)
    for (wlw_uint x = 0; x < args->width; x++)
      fb[y * args->width + x] = 0xFF000000
        | (((wlw_uint) (0xFF * (y * 1.f / args->height))) << 8)
        | (((wlw_uint) (0xFF * (x * 1.f / args->width))) << 0);
  wl_surface_r_attach (surface, buf, 0, 0);
  wl_surface_r_commit (surface);

  char text_buf[64];
  char *cursor = text_buf;

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
                if (ev->state.as_enum != wl_keyboard_key_state_released)
                  break;
                char key = wlw_evdev_to_ascii[ev->key];
                switch (wlw_evdev_to_ascii[ev->key])
                  {
                  case '\n':
                    *cursor = '\0';
                    printf ("%s\n", text_buf);
                    cursor = text_buf;
                    break;
                  case 'A' ... 'Z':
                    *(cursor++) = key + 32;
                    break;
                  case '\t':
                  case ' ':
                    *(cursor++) = key;
                    break;
                  }
                break;
              }
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
