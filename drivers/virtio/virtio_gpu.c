// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024, Jiaxun Yang <jiaxun.yang@flygoat.com>
 */

#define pr_fmt(fmt) "virtio_gpu: " fmt

#include <dm.h>
#include <log.h>
#include <malloc.h>
#include <video.h>
#include <virtio_types.h>
#include <virtio.h>
#include <virtio_ring.h>
#include "virtio_gpu.h"
#include <asm/io.h>

struct virtio_gpu_priv {
	struct virtqueue *vq;
	u32 scanout_res_id;
	u64 fence_id;
	bool in_sync;
};

static int virtio_gpu_do_req(struct udevice *dev,
			     enum virtio_gpu_ctrl_type type,
			     void *in, size_t in_size,
			     void *out, size_t out_size, bool flush)
{
	int ret;
	uint len;
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct virtio_sg in_sg;
	struct virtio_sg out_sg;
	struct virtio_sg *sgs[] = { &in_sg, &out_sg };
	struct virtio_gpu_ctrl_hdr *ctrl_hdr_in = in;
	struct virtio_gpu_ctrl_hdr *ctrl_hdr_out = out;

	ctrl_hdr_in->type = cpu_to_virtio32(dev, (u32)type);
	if (flush) {
		ctrl_hdr_in->flags = cpu_to_virtio32(dev, VIRTIO_GPU_FLAG_FENCE);
		ctrl_hdr_in->fence_id = cpu_to_virtio64(dev, priv->fence_id++);
	} else {
		ctrl_hdr_in->flags = 0;
		ctrl_hdr_in->fence_id = 0;
	}
	ctrl_hdr_in->ctx_id = 0;
	ctrl_hdr_in->ring_idx = 0;
	in_sg.addr = in;
	in_sg.length = in_size;
	out_sg.addr = out;
	out_sg.length = out_size;

	ret = virtqueue_add(priv->vq, sgs, 1, 1);
	if (ret) {
		log_debug("virtqueue_add failed %d\n", ret);
		return ret;
	}
	virtqueue_kick(priv->vq);

	debug("wait...");
	while (!virtqueue_get_buf(priv->vq, &len))
		;
	debug("done\n");

	if (out_size != len) {
		log_debug("Invalid response size %d, expected %d\n",
			  len, (uint)out_size);
	}

	return virtio32_to_cpu(dev, ctrl_hdr_out->type);
}

static int virtio_gpu_probe(struct udevice *dev)
{
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_gpu_ctrl_hdr ctrl_hdr_in;
	struct virtio_gpu_ctrl_hdr ctrl_hdr_out;
	struct virtio_gpu_resp_display_info *disp_info_out;
	struct virtio_gpu_display_one *disp;
	struct virtio_gpu_resource_create_2d *res_create_2d_in;
	void *res_buf_in;
	struct virtio_gpu_resource_attach_backing *res_attach_backing_in;
	struct virtio_gpu_mem_entry *mem_entry;
	struct virtio_gpu_set_scanout *set_scanout_in;
	unsigned int scanout_mask = 0;
	int ret, i;

	if (!plat->base) {
		log_warning("No framebuffer allocated\n");
		return -EINVAL;
	}

	ret = virtio_find_vqs(dev, 1, &priv->vq);
	if (ret < 0) {
		log_warning("virtio_find_vqs failed\n");
		return ret;
	}

	disp_info_out = malloc(sizeof(struct virtio_gpu_resp_display_info));
	ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_GET_DISPLAY_INFO, &ctrl_hdr_in,
				sizeof(struct virtio_gpu_ctrl_hdr), disp_info_out,
				sizeof(struct virtio_gpu_resp_display_info), false);

	if (ret != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
		log_warning("CMD_GET_DISPLAY_INFO failed %d\n", ret);
		ret = -EINVAL;
		goto out_free_disp;
	}

	for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
		disp = &disp_info_out->pmodes[i];
		if (!disp->enabled)
			continue;
		log_debug("Found available scanout: %d\n", i);
		scanout_mask |= 1 << i;
	}

	if (!scanout_mask) {
		log_warning("No active scanout found\n");
		ret = -EINVAL;
		goto out_free_disp;
	}

	free(disp_info_out);
	disp_info_out = NULL;

	/* TODO: We can parse EDID for those info */
	uc_priv->xsize = CONFIG_VAL(VIRTIO_GPU_SIZE_X);
	uc_priv->ysize = CONFIG_VAL(VIRTIO_GPU_SIZE_Y);
	uc_priv->bpix = VIDEO_BPP32;

	priv->scanout_res_id = 1;
	res_create_2d_in = malloc(sizeof(struct virtio_gpu_resource_create_2d));
	res_create_2d_in->resource_id = cpu_to_virtio32(dev, priv->scanout_res_id);
	res_create_2d_in->format = cpu_to_virtio32(dev, VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM);
	res_create_2d_in->width = cpu_to_virtio32(dev, uc_priv->xsize);
	res_create_2d_in->height = cpu_to_virtio32(dev, uc_priv->ysize);

	ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_RESOURCE_CREATE_2D, res_create_2d_in,
				sizeof(struct virtio_gpu_resource_create_2d), &ctrl_hdr_out,
				sizeof(struct virtio_gpu_ctrl_hdr), false);
	if (ret != VIRTIO_GPU_RESP_OK_NODATA) {
		log_warning("CMD_RESOURCE_CREATE_2D failed %d\n", ret);
		ret = -EINVAL;
		goto out_free_res_create_2d;
	}

	free(res_create_2d_in);
	res_create_2d_in = NULL;

	res_buf_in = malloc(sizeof(struct virtio_gpu_resource_attach_backing) +
			    sizeof(struct virtio_gpu_mem_entry));
	res_attach_backing_in = res_buf_in;
	mem_entry = res_buf_in + sizeof(struct virtio_gpu_resource_attach_backing);
	res_attach_backing_in->resource_id = cpu_to_virtio32(dev, priv->scanout_res_id);
	res_attach_backing_in->nr_entries = cpu_to_virtio32(dev, 1);
	mem_entry->addr = cpu_to_virtio64(dev, virt_to_phys((void *)plat->base));
	mem_entry->length = cpu_to_virtio32(dev, plat->size);
	mem_entry->padding = 0;

	ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING, res_buf_in,
				sizeof(struct virtio_gpu_resource_attach_backing) +
				sizeof(struct virtio_gpu_mem_entry), &ctrl_hdr_out,
				sizeof(struct virtio_gpu_ctrl_hdr), false);

	if (ret != VIRTIO_GPU_RESP_OK_NODATA) {
		log_warning("CMD_RESOURCE_ATTACH_BACKING failed %d\n", ret);
		ret = -EINVAL;
		goto out_free_res_buf;
	}
	free(res_buf_in);
	res_buf_in = NULL;

	set_scanout_in = malloc(sizeof(struct virtio_gpu_set_scanout));
	while (scanout_mask) {
		u32 active_scanout = ffs(scanout_mask) - 1;

		set_scanout_in->r.x = 0;
		set_scanout_in->r.y = 0;
		set_scanout_in->r.width = cpu_to_virtio32(dev, uc_priv->xsize);
		set_scanout_in->r.height = cpu_to_virtio32(dev, uc_priv->ysize);
		set_scanout_in->scanout_id = cpu_to_virtio32(dev, active_scanout);
		set_scanout_in->resource_id = cpu_to_virtio32(dev, priv->scanout_res_id);

		ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_SET_SCANOUT, set_scanout_in,
					sizeof(struct virtio_gpu_set_scanout), &ctrl_hdr_out,
					sizeof(struct virtio_gpu_ctrl_hdr), false);

		if (ret != VIRTIO_GPU_RESP_OK_NODATA) {
			log_warning("CMD_SET_SCANOUT failed %d for scanout %d\n",
				    ret, active_scanout);
			ret = -EINVAL;
			goto out_free_set_scanout;
		}
		scanout_mask &= ~(1 << active_scanout);
	}
	free(set_scanout_in);
	set_scanout_in = NULL;

	return 0;
out_free_set_scanout:
	if (set_scanout_in)
		free(set_scanout_in);
out_free_res_buf:
	if (res_buf_in)
		free(res_buf_in);
out_free_res_create_2d:
	if (res_create_2d_in)
		free(res_create_2d_in);
out_free_disp:
	if (disp_info_out)
		free(disp_info_out);
	return ret;
}

static int virtio_gpu_bind(struct udevice *dev)
{
	struct virtio_dev_priv *virtio_uc_priv = dev_get_uclass_priv(dev->parent);
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	/* Indicate what driver features we support */
	virtio_driver_features_init(virtio_uc_priv, NULL, 0, NULL, 0);
	plat->base = 0; /* Framebuffer will be allocated by the video-uclass */
	plat->size = CONFIG_VAL(VIRTIO_GPU_SIZE_X) *
		     CONFIG_VAL(VIRTIO_GPU_SIZE_X) * VNBYTES(VIDEO_BPP32);

	return 0;
}

static int virtio_gpu_video_sync(struct udevice *dev)
{
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct virtio_gpu_transfer_to_host_2d to_host_2d_in;
	struct virtio_gpu_resource_flush res_flush_in;
	struct virtio_gpu_ctrl_hdr ctrl_hdr_out;
	int ret;

	/* We need to protect sync function reentrance to prevent exausting VQ */
	if (priv->in_sync)
		return 0;

	priv->in_sync = true;

	to_host_2d_in.r.x = 0;
	to_host_2d_in.r.y = 0;
	to_host_2d_in.r.width = cpu_to_virtio32(dev, uc_priv->xsize);
	to_host_2d_in.r.height = cpu_to_virtio32(dev, uc_priv->ysize);
	to_host_2d_in.offset = 0;
	to_host_2d_in.resource_id = cpu_to_virtio32(dev, priv->scanout_res_id);
	to_host_2d_in.padding = 0;

	ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D, &to_host_2d_in,
				sizeof(struct virtio_gpu_transfer_to_host_2d), &ctrl_hdr_out,
				sizeof(struct virtio_gpu_ctrl_hdr), true);
	if (ret != VIRTIO_GPU_RESP_OK_NODATA) {
		log_debug("CMD_TRANSFER_TO_HOST_2D failed %d\n", ret);
		priv->in_sync = false;
		return -EINVAL;
	}

	res_flush_in.r.x = 0;
	res_flush_in.r.y = 0;
	res_flush_in.r.width = cpu_to_virtio32(dev, uc_priv->xsize);
	res_flush_in.r.height = cpu_to_virtio32(dev, uc_priv->ysize);
	res_flush_in.resource_id = cpu_to_virtio32(dev, priv->scanout_res_id);
	res_flush_in.padding = 0;

	ret = virtio_gpu_do_req(dev, VIRTIO_GPU_CMD_RESOURCE_FLUSH, &res_flush_in,
				sizeof(struct virtio_gpu_resource_flush), &ctrl_hdr_out,
				sizeof(struct virtio_gpu_ctrl_hdr), true);
	if (ret != VIRTIO_GPU_RESP_OK_NODATA) {
		log_debug("CMD_RESOURCE_FLUSH failed %d\n", ret);
		priv->in_sync = false;
		return -EINVAL;
	}

	priv->in_sync = false;
	return 0;
}

static struct video_ops virtio_gpu_ops = {
	.video_sync = virtio_gpu_video_sync,
};

U_BOOT_DRIVER(virtio_gpu) = {
	.name	= VIRTIO_GPU_DRV_NAME,
	.id	= UCLASS_VIDEO,
	.bind	= virtio_gpu_bind,
	.probe	= virtio_gpu_probe,
	.remove = virtio_reset,
	.ops	= &virtio_gpu_ops,
	.priv_auto	= sizeof(struct virtio_gpu_priv),
	.flags	= DM_FLAG_ACTIVE_DMA,
};
