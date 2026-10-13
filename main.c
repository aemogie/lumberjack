#define _GNU_SOURCE
#include "wlw.c"
#include <stdio.h>
#include <sys/mman.h>

static wlw_word should_stop;
static wlw_word compositor;
static wlw_word seat;
static wlw_word surface;
static wlw_word wm_base;
static wlw_word shm;
static wlw_word width, height;
static wlw_word pending_width, pending_height;

struct face
{
  wlw_word pool;
  wlw_word buffer;
  int fd;
  int cap;
  uint32_t *fb;
};

#define MAX_FACES 2
static struct face faces[MAX_FACES] = { 0 };

static int front = -1;

void
on_error (const struct wlw_untyped *msg)
{
  const struct wl_display_e_error *error = (const void *) msg;
  const char *obj_name =
    wlw_interface_names[wlw_typeof[error->object_id]].str;
  fprintf (stderr, "wl error (%s.%d): %s\n", obj_name, error->code,
           error->message.str);
}

void
initialise_everything (void)
{
  struct
  {
    struct wl_compositor_r_create_surface surface;
    struct xdg_wm_base_r_get_xdg_surface xdg_surface;
    struct xdg_surface_r_get_toplevel toplevel;
    struct wl_shm_r_create_pool pools[MAX_FACES];
    struct wl_surface_r_commit commit;
  } msgs;
  int face;

  msgs.surface.hdr = wlw_make_header (compositor, sizeof (msgs.surface),
                                      wl_compositor_r_create_surface_op);
  msgs.surface.id = wlw_genid (wl_surface_i);
  surface = msgs.surface.id;

  msgs.xdg_surface.hdr =
    wlw_make_header (wm_base, sizeof (msgs.xdg_surface),
                     xdg_wm_base_r_get_xdg_surface_op);
  msgs.xdg_surface.id = wlw_genid (xdg_surface_i);
  msgs.xdg_surface.surface = msgs.surface.id;

  msgs.toplevel.hdr =
    wlw_make_header (msgs.xdg_surface.id, sizeof (msgs.toplevel),
                     xdg_surface_r_get_toplevel_op);
  msgs.toplevel.id = wlw_genid (xdg_toplevel_i);

  for (face = 0; face < MAX_FACES; face++)
    {
      msgs.pools[face].hdr =
        wlw_make_header (shm, sizeof (msgs.pools[face]),
                         wl_shm_r_create_pool_op);
      msgs.pools[face].id = wlw_genid (wl_shm_pool_i);
      msgs.pools[face].size = 1;
      faces[face].pool = msgs.pools[face].id;

      faces[face].fd = memfd_create ("wl_shm", MFD_CLOEXEC);
      ftruncate (faces[face].fd, 1);
      faces[face].fb =
        mmap (NULL, 1, PROT_READ | PROT_WRITE, MAP_SHARED, faces[face].fd, 0);
    }

  msgs.commit.hdr = wlw_make_header (msgs.surface.id, sizeof (msgs.commit),
                                     wl_surface_r_commit_op);

  {
    /* the ugliest syscall so far ive had to use */

    struct msghdr prep;
    struct iovec iov;
    struct cmsghdr *cmsg = NULL;
    int *fds;
    int i;
    /* enough bloat to hold just 1 fd */
    char buf[CMSG_SPACE (sizeof (int) * MAX_FACES)];

    /* one singular iovec */
    iov.iov_base = &msgs;
    iov.iov_len = sizeof (msgs);
    prep.msg_iov = &iov;
    prep.msg_iovlen = 1;

    prep.msg_control = buf;
    prep.msg_controllen = sizeof (buf);

    cmsg = CMSG_FIRSTHDR (&prep);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN (sizeof (int) * MAX_FACES);
    fds = (int *) CMSG_DATA (cmsg);
    for (i = 0; i < MAX_FACES; i++)
      {
        fds[i] = faces[i].fd;
      }

    sendmsg (wlw_sockfd, &prep, 0);
  }
}

void
on_registry_global (const struct wlw_untyped *msg)
{
  struct wl_registry_e_global_ptrs ptrs = wl_registry_e_global (msg);
  const char *iname = ptrs.interface->str;

  if (!compositor
      && !strcmp (iname, wlw_interface_names[wl_compositor_i].str))
    compositor = wl_registry_r_bind (msg->hdr.object, wl_compositor_i, ptrs);
  else if (!seat && !strcmp (iname, wlw_interface_names[wl_seat_i].str))
    seat = wl_registry_r_bind (msg->hdr.object, wl_seat_i, ptrs);
  else if (!wm_base
           && !strcmp (iname, wlw_interface_names[xdg_wm_base_i].str))
    wm_base = wl_registry_r_bind (msg->hdr.object, xdg_wm_base_i, ptrs);
  else if (!shm && !strcmp (iname, wlw_interface_names[wl_shm_i].str))
    shm = wl_registry_r_bind (msg->hdr.object, wl_shm_i, ptrs);
  else
    return;

  if (compositor && seat && wm_base && shm)
    initialise_everything ();
}

void
on_toplevel_configure (const struct wlw_untyped *msg)
{
  const struct xdg_toplevel_e_configure *event = (const void *) msg->payload;
  pending_width = event->width;
  pending_height = event->height;
}

void
clear_front (void)
{
  wlw_word x, y;
  for (x = 0; x < width; x++)
    for (y = 0; y < height; y++)
      {
        faces[front].fb[y * width + x] = 0xFFFF0000;
      }
}

void
resize_window (void)
{
  int nxt = (front + 1) % MAX_FACES;
  struct
  {
    struct wl_buffer_r_destroy destroy;
    struct wl_shm_pool_r_resize resize;
    struct wl_shm_pool_r_create_buffer buffer;
    struct wl_surface_r_attach attach;
    struct wl_surface_r_damage_buffer damage;
    struct wl_surface_r_commit commit;
  } msgs;
  int msgs_start = 0;
  int new_size = pending_width * pending_height * 4;

  if (faces[front].buffer)
    msgs.destroy.hdr =
      wlw_make_header (faces[front].buffer, sizeof (msgs.destroy),
                       wl_buffer_r_destroy_op);
  else
    {
      msgs_start = offsetof (__typeof__ (msgs), resize);
    }

  if (new_size > faces[nxt].cap)
    {
      ftruncate (faces[nxt].fd, new_size);
      faces[nxt].fb =
        mremap (faces[nxt].fb, faces[nxt].cap, new_size, MREMAP_MAYMOVE);
      msgs.resize.hdr =
        wlw_make_header (faces[nxt].pool, sizeof (msgs.resize),
                         wl_shm_pool_r_resize_op);
      msgs.resize.size = new_size;
      faces[nxt].cap = new_size;
    }
  else                          /* skip resize */
    msgs_start = offsetof (__typeof__ (msgs), buffer);

  msgs.buffer.hdr =
    wlw_make_header (faces[nxt].pool, sizeof (msgs.buffer),
                     wl_shm_pool_r_create_buffer_op);
  if (!faces[nxt].buffer)
    faces[nxt].buffer = wlw_genid (wl_buffer_i);
  msgs.buffer.id = faces[nxt].buffer;
  msgs.buffer.offset = 0;
  msgs.buffer.width = pending_width;
  msgs.buffer.height = pending_height;
  msgs.buffer.stride = pending_width * 4;
  msgs.buffer.format = wl_shm_format_argb8888;

  msgs.attach.hdr =
    wlw_make_header (surface, sizeof (msgs.attach), wl_surface_r_attach_op);
  msgs.attach.buffer = faces[nxt].buffer;
  msgs.attach.x = 0;
  msgs.attach.y = 0;

  msgs.damage.hdr =
    wlw_make_header (surface, sizeof (msgs.damage),
                     wl_surface_r_damage_buffer_op);
  msgs.damage.x = 0;
  msgs.damage.y = 0;
  msgs.damage.width = pending_width;
  msgs.damage.height = pending_height;

  msgs.commit.hdr =
    wlw_make_header (surface, sizeof (msgs.commit), wl_surface_r_commit_op);

  write (wlw_sockfd, ((char *) &msgs) + msgs_start,
         sizeof (msgs) - msgs_start);

  width = pending_width;
  height = pending_height;
  front = nxt;

  clear_front ();
}

void
on_configure (const struct wlw_untyped *msg)
{
  const struct xdg_surface_e_configure *event = (const void *) msg;
  struct xdg_surface_r_ack_configure request;

  if (width != pending_width || height != pending_height)
    {
      resize_window ();
    }

  request.hdr =
    wlw_make_header (msg->hdr.object, sizeof (request),
                     xdg_surface_r_ack_configure_op);
  request.serial = event->serial;
  write (wlw_sockfd, &request, sizeof (request));
}

void
on_close (const struct wlw_untyped *msg)
{
  should_stop = 1;
  (void) msg;
}

int
main (void)
{
  wlw_register (wl_display_i, wl_display_e_error_op, on_error);
  wlw_register (wl_registry_i, wl_registry_e_global_op, on_registry_global);
  wlw_register (xdg_surface_i, xdg_surface_e_configure_op, on_configure);
  wlw_register (xdg_toplevel_i, xdg_toplevel_e_configure_op,
                on_toplevel_configure);
  wlw_register (xdg_toplevel_i, xdg_toplevel_e_close_op, on_close);

  wlw_open ();
  {
    struct wl_display_r_get_registry msg;
    msg.hdr = wlw_make_header (1, sizeof (msg), wl_display_r_get_registry_op);
    msg.registry = wlw_genid (wl_registry_i);
    write (wlw_sockfd, &msg, sizeof (msg));
  }
  while (!should_stop)
    wlw_dispatch (wlw_recv ());

  return 0;
}
