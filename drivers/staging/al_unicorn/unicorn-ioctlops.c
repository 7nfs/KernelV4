/*
    unicorn.c - V4L2 driver for unicorn

    Copyright (c) 2010 Aldebaran robotics
    joseph pinkasfeld joseph.pinkasfeld@gmail.com
    Ludovic SMAL <lsmal@aldebaran-robotics.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "unicorn.h"

#ifdef  CONFIG_AL_UNICORN_WIDTH_VIDEO_SUPPORT
#include "unicorn-video.h"


#define FORMAT_FLAGS_PACKED       0x01

struct unicorn_fmt formats[] = {
{
  .name = "4:2:2, packed, YUYV",
  .fourcc = V4L2_PIX_FMT_YUYV,
  .depth = 16,
  .flags = FORMAT_FLAGS_PACKED,
},
};

int get_format_size(void)
{
  return ARRAY_SIZE(formats);
}

struct unicorn_fmt *format_by_fourcc(unsigned int fourcc)
{
  unsigned int i;

  for (i = 0; i < ARRAY_SIZE(formats); i++)
  {
    if (formats[i].fourcc == fourcc)
    {
      return formats + i;
    }
  }
  return NULL;
}

static int vidioc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
  struct unicorn_dev *dev = ((struct unicorn_fh *)priv)->dev;

  strcpy(cap->driver, "unicorn");
  strlcpy(cap->card, dev->name, sizeof(cap->card));
  sprintf(cap->bus_info, "PCIe:%s", pci_name(dev->pci));
  cap->version = UNICORN_VERSION_CODE;
  cap->capabilities =
      V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;

  return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
          struct v4l2_fmtdesc *f)
{
  if (unlikely(f->index >= ARRAY_SIZE(formats)))
    return -EINVAL;

  strlcpy(f->description, formats[f->index].name, sizeof(f->description));
  f->pixelformat = formats[f->index].fourcc;

  return 0;
}


static int vidioc_try_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
  struct unicorn_fmt *fmt;
  enum v4l2_field field;
  unsigned int maxw, maxh;
  unsigned int minw, minh;

  fmt = format_by_fourcc(f->fmt.pix.pixelformat);
  if (NULL == fmt)
  {
    printk(KERN_INFO "%s format_by_fourcc_fail fmt == NULL format %d \n",__func__,f->fmt.pix.pixelformat);
    return -EINVAL;
  }
  field = f->fmt.pix.field;
  maxw = 1280;
  maxh = 960;
  minw = 160;
  minh = 120;

  if (f->fmt.pix.height < minh)
    f->fmt.pix.height = minh;
  if (f->fmt.pix.height > maxh)
    f->fmt.pix.height = maxh;
  if (f->fmt.pix.width < minw)
    f->fmt.pix.width = minw;
  if (f->fmt.pix.width > maxw)
    f->fmt.pix.width = maxw;

  f->fmt.pix.bytesperline = (f->fmt.pix.width * fmt->depth) >> 3;
  f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;

  return 0;
}

int vidioc_g_fmt_vid_cap(struct file *file, void *priv, struct v4l2_format *f)
{
  struct unicorn_fh *fh = priv;

  f->fmt.pix.width = fh->width;
  f->fmt.pix.height = fh->height;
  f->fmt.pix.field = fh->vidq.field;
  f->fmt.pix.pixelformat = fh->fmt->fourcc;
  f->fmt.pix.bytesperline = (f->fmt.pix.width * fh->fmt->depth) >> 3;
  f->fmt.pix.sizeimage = f->fmt.pix.height * f->fmt.pix.bytesperline;

  return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv, struct v4l2_requestbuffers *p)
{
  struct unicorn_fh *fh = priv;
  return videobuf_reqbufs(&fh->vidq, p);
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
  struct unicorn_fh *fh = priv;
  return videobuf_querybuf(&fh->vidq, p);
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
  struct unicorn_fh *fh = priv;
  return videobuf_qbuf(&fh->vidq, p);
}

static int unicorn_enum_input(struct unicorn_dev *dev, struct v4l2_input *i)
{
  static const char *iname[MAX_VIDEO_INPUT_ENTRY] = {
    [0] = "input 0",
    [1] = "input 1",
    [2] = "input 2",
    [3] = "input 3",
    [4] = "Mire"
  };
  unsigned int n;
  dprintk_video(1, dev->name, "%s() index:%d\n", __func__, i->index);

  n = i->index;
  if (n > MAX_VIDEO_INPUT_ENTRY-1)
    return -EINVAL;

  memset(i, 0, sizeof(*i));
  i->index = n;
  i->type = V4L2_INPUT_TYPE_CAMERA;
  strcpy(i->name, iname[n]);

  i->std = V4L2_STD_UNKNOWN;
  return 0;
}


static int vidioc_enum_input(struct file *file, void *priv, struct v4l2_input *i)
{
  struct unicorn_dev *dev = ((struct unicorn_fh *)priv)->dev;
  dprintk_video(1, dev->name, "%s()\n", __func__);
  return unicorn_enum_input(dev, i);
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;
  *i = fh->input;
  dprintk_video(1, dev->name, "%s() returns %d\n", __func__, *i);
  return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;
  int err;

  dprintk_video(1, dev->name, "%s(%d)\n", __func__, i);

  if (fh) {
    err = v4l2_prio_check(&dev->prio, &fh->prio);
    if (0 != err)
      return err;
  }

  if (i > MAX_VIDEO_INPUT_ENTRY-1) {
    printk(KERN_INFO "%s() invalid input -EINVAL\n", __func__);
    return -EINVAL;
  }

  if(fh->channel > MAX_VID_CHANNEL_NUM)
  {
    printk(KERN_INFO "%s()  invalid channel -EINVAL\n", __func__);
    return -EINVAL;
  }

  fh->input = i;

  return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv, struct v4l2_control *ctl)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = (fh)->dev;

  v4l2_subdev_call(dev->sensor[fh->input], core, g_ctrl, ctl);

  return 0;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
       struct v4l2_control *ctl)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;
  int err;

  if (fh) {
    err = v4l2_prio_check(&dev->prio, &fh->prio);
    if (0 != err)
      return err;
  }

  return v4l2_subdev_call(dev->sensor[fh->input], core, s_ctrl, ctl);
}


static int vidioc_g_parm(struct file *filp, void *priv,
      struct v4l2_streamparm *parm)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;

  struct v4l2_captureparm *cp = &parm->parm.capture;
  struct v4l2_fract *tpf = &cp->timeperframe;

  tpf->denominator = dev->fps_limit[fh->channel];
  tpf->numerator = 1;

  //return v4l2_subdev_call(dev->sensor[fh->input], video, g_parm, parm);
  return 0;
}

static int vidioc_s_parm(struct file *filp, void *priv,
      struct v4l2_streamparm *parm)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;

  struct v4l2_captureparm *cp = &parm->parm.capture;
  struct v4l2_fract *tpf = &cp->timeperframe;

  spin_lock(&dev->slock);
  dev->fps_limit[fh->channel]=tpf->denominator;
  spin_unlock(&dev->slock);

#ifdef CONFIG_AL_UNICORN_FPS_ON_THE_FLY
  unicorn_video_change_fps(dev,fh);
#endif

  //return v4l2_subdev_call(dev->sensor[fh->input], video, s_parm, parm);
  return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
         struct v4l2_queryctrl *qctrl)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;
  return v4l2_subdev_call(dev->sensor[fh->input], core, queryctrl, qctrl);
}

static int vidioc_g_priority(struct file *file, void * priv, enum v4l2_priority *p)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;

  *p = v4l2_prio_max(&dev->prio);

  return 0;
}

static int vidioc_s_priority(struct file *file, void *f, enum v4l2_priority prio)
{
  struct unicorn_fh *fh = f;
  struct unicorn_dev *dev = ((struct unicorn_fh *)f)->dev;

  return v4l2_prio_change(&dev->prio, &fh->prio, prio);
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
        struct v4l2_format *f)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = ((struct unicorn_fh *)priv)->dev;
  int err;

  if (fh) {
    err = v4l2_prio_check(&dev->prio, &fh->prio);
    if (0 != err)
    {
        printk(KERN_INFO "%s()v4l2_prio_check fail \n", __func__);
        return err;
    }
  }

  dprintk_video(1, dev->name, "%s()\n", __func__);

  if(fh->input == MIRE_VIDEO_INPUT)
  {
    dprintk_video(1, dev->name, "%s() MIRE input selected\n", __func__);
  }
  else if(dev->sensor[fh->input]==NULL)
  {
    printk(KERN_INFO "%s() no device on this input %d \n", __func__,fh->input);
    return -EINVAL;
  }

  err = vidioc_try_fmt_vid_cap(file, priv, f);

  if (0 != err)
  {
    printk(KERN_WARNING "%s() vidioc_try_fmt_vid_cap fail\n", __func__);
    return err;
  }

  fh->fmt = format_by_fourcc(f->fmt.pix.pixelformat);
  fh->vidq.field = f->fmt.pix.field;

  if (fh->fmt->fourcc != V4L2_PIX_FMT_YUYV){
    printk(KERN_INFO "%s() expected 0x%x  \n", __func__,V4L2_PIX_FMT_YUYV);
    printk(KERN_INFO "%s()0x%x pixel format not supported \n", __func__,fh->fmt->fourcc);
    return -EINVAL;
  }

  if(fh->input == MIRE_VIDEO_INPUT)
  {
    err = 0;
  }
  else
  {
    err = v4l2_subdev_call(dev->sensor[fh->input], video, s_fmt, f);
  }


  fh->width = f->fmt.pix.width;
  fh->height = f->fmt.pix.height;

  dprintk_video(1, dev->name, "%s() width=%d height=%d field=%d err=%d\n", __func__, fh->width,
    fh->height, fh->vidq.field, err);

  return err;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
  int ret = 0;
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;

  ret = videobuf_dqbuf(&fh->vidq, p, file->f_flags & O_NONBLOCK);

  p->sequence = dev->vidq[fh->channel].count;

  return ret;
}

// resource management
int res_get(struct unicorn_dev *dev, struct unicorn_fh *fh, unsigned int bit)
{
  dprintk_video(1, dev->name, "%s()\n", __func__);
  if (fh->resources & bit)
    /* have it already allocated */
    return 1;

  /* is it free? */
  mutex_lock(&dev->mutex);
  if (dev->resources & bit) {
    /* no, someone else uses it */
    mutex_unlock(&dev->mutex);
    return 0;
  }
  /* it's free, grab it */
  fh->resources |= bit;
  dev->resources |= bit;
  dprintk_video(1, dev->name, "res: get %d\n", bit);
  mutex_unlock(&dev->mutex);
  return 1;
}

int res_check(struct unicorn_fh *fh, unsigned int bit)
{
  return fh->resources & bit;
}

int res_locked(struct unicorn_dev *dev, unsigned int bit)
{
  return dev->resources & bit;
}

void res_free(struct unicorn_dev *dev, struct unicorn_fh *fh, unsigned int bits)
{
  BUG_ON((fh->resources & bits) != bits);
  dprintk_video(1, dev->name, "%s()\n", __func__);

  mutex_lock(&dev->mutex);
  fh->resources &= ~bits;
  dev->resources &= ~bits;
  dprintk_video(1, dev->name, "res: put %d\n", bits);
  mutex_unlock(&dev->mutex);
}

int get_resource(struct unicorn_fh *fh, int resource)
{
  switch (fh->type) {
  case V4L2_BUF_TYPE_VIDEO_CAPTURE:
    return resource;
  default:
    BUG();
    return 0;
  }
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;

  if (unlikely(fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
    return -EINVAL;
  }

  if (unlikely(i != fh->type)) {
    return -EINVAL;
  }

  if (unlikely(!res_get(dev, fh, get_resource(fh, 0x01 << fh->channel)))) {
    return -EBUSY;
  }

  return videobuf_streamon(&fh->vidq);
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = fh->dev;
  int err, res;

  if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
    return -EINVAL;
  if (i != fh->type)
    return -EINVAL;

  res = get_resource(fh,  0x01 << fh->channel);
  err = videobuf_streamoff(&fh->vidq);
  if (err < 0)
    return err;
  res_free(dev, fh, res);
  return 0;
}

static int vidioc_log_status(struct file *file, void *priv)
{
  struct unicorn_fh *fh = (struct unicorn_fh *)priv;
  struct unicorn_dev *dev = fh->dev;
  char name[32 + 2];

  snprintf(name, sizeof(name), "%s/2", dev->name);
  printk(KERN_INFO "%s/2: ============  START LOG STATUS  ============\n",
         dev->name);
  printk(KERN_INFO "DMA%d is %s\n",fh->channel,
         (dev->pcie_dma->dma[fh->channel].ctrl & DMA_CONTROL_RESET_MASK ) ? "reset" : "enable");
  printk(KERN_INFO "Video channel %d is %s\n",fh->channel,
         (dev->global_register->video[fh->channel].ctrl & VIDEO_CONTROL_ENABLE_MASK ) ? "enable" : "disable");
  printk(KERN_INFO "video input %d reset = 0x%X\n",fh->input,
         dev->global_register->video_in_reset);
  printk(KERN_INFO "%s/2: =============  END LOG STATUS  =============\n",
         dev->name);
  return 0;
}


static int vidioc_g_std(struct file *file, void *fh, v4l2_std_id *std)
{
  return 0;
}

int vidioc_s_std(struct file *file, void *fh, v4l2_std_id *std)
{
  return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int vidioc_g_register(struct file *file, void *priv, struct v4l2_dbg_register *reg)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = (fh)->dev;
  int err;

  if (fh) {
    err = v4l2_prio_check(&dev->prio, &fh->prio);
    if (0 != err)
      return err;
  }
  
  return v4l2_subdev_call(dev->sensor[fh->input], core, g_register, reg);
}

static int vidioc_s_register(struct file *file, void *priv, struct v4l2_dbg_register *reg)
{
  struct unicorn_fh *fh = priv;
  struct unicorn_dev *dev = (fh)->dev;
  int err;

  if (fh) {
    err = v4l2_prio_check(&dev->prio, &fh->prio);
    if (0 != err)
      return err;
  }

  return v4l2_subdev_call(dev->sensor[fh->input], core, s_register, reg);
}
#endif

const struct v4l2_ioctl_ops video_ioctl_ops = {
  .vidioc_querycap = vidioc_querycap, /* done */
  .vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,  /* done */
  .vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap, /* done */
  .vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap, /* done */
  .vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap, /* done */
  .vidioc_reqbufs = vidioc_reqbufs, /* done */
  .vidioc_querybuf = vidioc_querybuf, /* done */
  .vidioc_qbuf = vidioc_qbuf, /* done */
  .vidioc_dqbuf = vidioc_dqbuf, /* done */
 // .vidioc_cropcap = vidioc_cropcap, deleted
 // .vidioc_s_crop = vidioc_s_crop, deleted
 // .vidioc_g_crop = vidioc_g_crop, deleted
  .vidioc_enum_input = vidioc_enum_input, /* done */
  .vidioc_g_input = vidioc_g_input, /* done */
  .vidioc_s_input = vidioc_s_input, /* done */
  .vidioc_g_ctrl = vidioc_g_ctrl,    /* ov7670done */
  .vidioc_s_ctrl = vidioc_s_ctrl,    /* ov7670done */
  .vidioc_queryctrl = vidioc_queryctrl,   /* ov7670done */
  .vidioc_streamon = vidioc_streamon, /* done */
  .vidioc_streamoff = vidioc_streamoff, /* done */
  .vidioc_log_status = vidioc_log_status, /* done */
  .vidioc_g_priority = vidioc_g_priority,
  .vidioc_s_priority = vidioc_s_priority,
  .vidioc_s_parm = vidioc_s_parm,
  .vidioc_g_parm = vidioc_g_parm,
  .vidioc_g_std = vidioc_g_std,
  .vidioc_s_std = vidioc_s_std,
#ifdef CONFIG_VIDEO_ADV_DEBUG
  .vidioc_g_register = vidioc_g_register,
  .vidioc_s_register = vidioc_s_register,
#endif
};
#endif
