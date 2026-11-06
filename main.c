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

#define MAX_FACES 3
static struct face faces[MAX_FACES] = { 0 };

static int front = -1;

static void
on_error (const struct wlw_untyped *msg)
{
  const struct wl_display_e_error *error = (const void *) msg;
  const char *obj_name =
    wlw_interface_names[wlw_typeof[error->object_id]].str;
  fprintf (stderr, "wl error (%s.%d): %s\n", obj_name, error->code,
           error->message.str);
  exit (error->code);
}

static void
initialise_everything (void)
{
  struct
  {
    struct wl_compositor_r_create_surface surface;
    struct xdg_wm_base_r_get_xdg_surface xdg_surface;
    struct xdg_surface_r_get_toplevel toplevel;
    struct wl_shm_r_create_pool pools[MAX_FACES];
    struct wl_shm_pool_r_create_buffer buffers[MAX_FACES];
    struct wl_surface_r_commit commit;
  } tx = { 0 };
  int i;

  tx.surface.hdr = wlw_make_header (compositor, sizeof (tx.surface),
                                    wl_compositor_r_create_surface_op);
  tx.surface.id = wlw_genid (wl_surface_i);
  surface = tx.surface.id;

  tx.xdg_surface.hdr =
    wlw_make_header (wm_base, sizeof (tx.xdg_surface),
                     xdg_wm_base_r_get_xdg_surface_op);
  tx.xdg_surface.id = wlw_genid (xdg_surface_i);
  tx.xdg_surface.surface = tx.surface.id;

  tx.toplevel.hdr =
    wlw_make_header (tx.xdg_surface.id, sizeof (tx.toplevel),
                     xdg_surface_r_get_toplevel_op);
  tx.toplevel.id = wlw_genid (xdg_toplevel_i);

  for (i = 0; i < MAX_FACES; i++)
    {
      struct wl_shm_r_create_pool *pool = &tx.pools[i];
      struct face *face = &faces[i];

      pool->hdr =
        wlw_make_header (shm, sizeof (tx.pools[i]), wl_shm_r_create_pool_op);
      pool->id = wlw_genid (wl_shm_pool_i);
      pool->size = 4;

      face->pool = pool->id;
      face->cap = pool->size;
      face->fd = memfd_create ("wl_shm", MFD_CLOEXEC);
      ftruncate (face->fd, face->cap);
      face->fb =
        mmap (NULL, face->cap, PROT_READ | PROT_WRITE, MAP_SHARED, face->fd,
              0);
    }

  /* for some reason either hyprland/libwayland-server expects all ids
     to be linearly increasing.  so the order you call wlw_genid()
     should match the same order you layout your messages.  as the
     messages in tx above are pool*N then buf*N, you cant do
     (pool+buf)*N as a single loop. */
  for (i = 0; i < MAX_FACES; i++)
    {
      struct wl_shm_pool_r_create_buffer *buf = &tx.buffers[i];
      struct face *face = &faces[i];

      buf->hdr =
        wlw_make_header (face->pool, sizeof (tx.buffers[i]),
                         wl_shm_pool_r_create_buffer_op);
      buf->id = wlw_genid (wl_buffer_i);
      buf->offset = 0;
      buf->width = 1;
      buf->height = 1;
      buf->stride = 4;
      buf->format = wl_shm_format_argb8888;
      face->buffer = buf->id;
    }

  tx.commit.hdr = wlw_make_header (tx.surface.id, sizeof (tx.commit),
                                   wl_surface_r_commit_op);

  {
    /* the ugliest syscall so far ive had to use */

    struct msghdr prep = { 0 };
    struct iovec iov;
    struct cmsghdr *cmsg = NULL;

    int fds[MAX_FACES];
    /* just alloacting (struct cmsghdr + fds), both aligned to size_t  */
    char buf[CMSG_SPACE (sizeof (fds))] = { 0 };
    int i;
    for (i = 0; i < MAX_FACES; i++)
      fds[i] = faces[i].fd;

    /* one singular iovec */
    iov.iov_base = &tx;
    iov.iov_len = sizeof (tx);
    prep.msg_iov = &iov;
    prep.msg_iovlen = 1;

    prep.msg_control = buf;
    prep.msg_controllen = sizeof (buf);

    /* basically dereferencing prep.msg_control, which is buf */
    cmsg = CMSG_FIRSTHDR (&prep);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN (sizeof (fds));
    /* CMSG_DATA(cmsg) should be just (void*)(cmsg[1]),
       aka (void*)((struct cmsghdr*)buf)[1] */
    memcpy (CMSG_DATA (cmsg), fds, sizeof (fds));

    sendmsg (wlw_sockfd, &prep, 0);
  }
}

static void
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

static void
on_toplevel_configure (const struct wlw_untyped *msg)
{
  const struct xdg_toplevel_e_configure *event = (const void *) msg->payload;
  pending_width = event->width;
  pending_height = event->height;
}

static void
clear_front (void)
{
  wlw_word x, y;
  for (x = 0; x < width; x++)
    for (y = 0; y < height; y++)
      {
        uint8_t r = 0, g = 0, b = 0;
        r = front == 0 ? 0xFF : 0x00;
        g = front == 1 ? 0xFF : 0x00;
        b = front == 2 ? 0xFF : 0x00;
        faces[front].fb[y * width + x] = 0xFF << 24
          | r << 16 | g << 8 | b << 0;
      }
}

static void
resize_window (void)
{
  struct
  {
    struct wl_shm_pool_r_resize resize;
    struct wl_buffer_r_destroy destroy;
    struct wl_shm_pool_r_create_buffer buffer;
    struct wl_surface_r_attach attach;
    struct wl_surface_r_damage_buffer damage;
    struct wl_surface_r_commit commit;
  } tx;
  int tx_start = 0;
  int new_size = pending_width * pending_height * 4;
  int next = (front + 1) % MAX_FACES;

  if (new_size > faces[next].cap)
    {
      ftruncate (faces[next].fd, new_size);
      faces[next].fb =
        mremap (faces[next].fb, faces[next].cap, new_size, MREMAP_MAYMOVE);
      tx.resize.hdr =
        wlw_make_header (faces[next].pool, sizeof (tx.resize),
                         wl_shm_pool_r_resize_op);
      tx.resize.size = new_size;
      faces[next].cap = new_size;
    }
  else
    tx_start = offsetof (__typeof__ (tx), destroy);

  tx.destroy.hdr =
    wlw_make_header (faces[next].buffer, sizeof (tx.destroy),
                     wl_buffer_r_destroy_op);

  tx.buffer.hdr =
    wlw_make_header (faces[next].pool, sizeof (tx.buffer),
                     wl_shm_pool_r_create_buffer_op);
  tx.buffer.id = faces[next].buffer;
  tx.buffer.offset = 0;
  tx.buffer.width = pending_width;
  tx.buffer.height = pending_height;
  tx.buffer.stride = pending_width * 4;
  tx.buffer.format = wl_shm_format_argb8888;

  tx.attach.hdr =
    wlw_make_header (surface, sizeof (tx.attach), wl_surface_r_attach_op);
  tx.attach.buffer = faces[next].buffer;
  tx.attach.x = 0;
  tx.attach.y = 0;

  tx.damage.hdr =
    wlw_make_header (surface, sizeof (tx.damage),
                     wl_surface_r_damage_buffer_op);
  tx.damage.x = 0;
  tx.damage.y = 0;
  tx.damage.width = pending_width;
  tx.damage.height = pending_height;

  tx.commit.hdr =
    wlw_make_header (surface, sizeof (tx.commit), wl_surface_r_commit_op);

  width = pending_width;
  height = pending_height;
  front = next;
  clear_front ();

  write (wlw_sockfd, ((char *) &tx) + tx_start, sizeof (tx) - tx_start);
}

static void
on_configure (const struct wlw_untyped *msg)
{
  const struct xdg_surface_e_configure *event = (const void *) msg;
  struct xdg_surface_r_ack_configure request;

  if (width != pending_width || height != pending_height)
    resize_window ();

  request.hdr =
    wlw_make_header (msg->hdr.object, sizeof (request),
                     xdg_surface_r_ack_configure_op);
  request.serial = event->serial;
  write (wlw_sockfd, &request, sizeof (request));
}

static void
on_ping (const struct wlw_untyped *msg)
{
  struct xdg_wm_base_e_ping *ping = (void *) msg->payload;
  struct xdg_wm_base_r_pong pong;
  pong.hdr =
    wlw_make_header (msg->hdr.object, sizeof (pong), xdg_wm_base_r_pong_op);
  pong.serial = ping->serial;
  write (wlw_sockfd, &pong, sizeof (pong));
}

static void
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
  wlw_register (xdg_wm_base_i, xdg_wm_base_e_ping_op, on_ping);

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
