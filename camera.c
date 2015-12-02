/*
 * camera driver for a Greybus module.
 *
 * Copyright 2015 Google Inc.
 *
 * Released under the GPLv2 only.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

#include "greybus.h"

struct gb_camera {
	struct gb_connection *connection;
    int     state;
};

struct gb_camera *camera_gb;

/**
 * @brief Get Camera capabilities
 */
static int gb_camera_capabilities(struct gb_camera *gb_cam, u16 *size,
                                  u8 *capabilities)
{
	struct gb_camera_capabilities_response *response;
    u32 max_cap_len, resp_size;
	int ret;

    max_cap_len = 16; /* max capabilities data size */
    resp_size = sizeof(*response) + max_cap_len;
    response = kzalloc(resp_size, GFP_KERNEL);

    if (!response) {
        return -ENOMEM;
    }

	ret = gb_operation_sync(gb_cam->connection, GB_CAMERA_TYPE_CAPABILITIES,
                            NULL, 0,
                            response, resp_size);
	if (ret) {
		goto err_free_mem;
    }

    /* todo: the code to handle capabilities response */
    *size = le16_to_cpu(response->size);
    memcpy(capabilities, response->capabilities, *size);

    kfree(response);
	return 0;

err_free_mem:
    kfree(response);
    return ret;
}

/**
 * @brief Configure camera module streams
 */
static int gb_camera_configure_stream(struct gb_camera *gb_cam, u16 num_streams,
                                      struct gb_stream_config_resp *stream)
{
    struct gb_camera_configure_streams_request *request;
	struct gb_camera_configure_streams_response *response;
    int i, req_size, resp_size;
	int ret;

    /* setup request */
    req_size = sizeof(*request) +
               num_streams * sizeof(struct gb_stream_config_req);
    request = kzalloc(req_size, GFP_KERNEL);
    if (!request) {
        return -ENOMEM;
    }

    request->num_streams = cpu_to_le16(num_streams);
    if (num_streams) {
        for (i = 0; i < num_streams; i++) {
            request->config[i].width = cpu_to_le16(stream[i].width);
            request->config[i].height = cpu_to_le16(stream[i].height);
            request->config[i].format = cpu_to_le16(stream[i].format);
        }
    }

    /* prepare response */
    resp_size = sizeof(*response) +
                num_streams * sizeof(struct gb_stream_config_resp);
    response = kzalloc(resp_size, GFP_KERNEL);
    if (!response) {
        ret = -ENOMEM;
        goto err_free_req;
    }

    /* send out request */
	ret = gb_operation_sync(gb_cam->connection, GB_CAMERA_TYPE_CONFIGURE_STREAMS,
                            request, req_size,
                            response, resp_size);
	if (ret) {
        goto err_free_resp;
    }

    /* todo: the code to handle configure stream */
    if (num_streams) {
        for (i = 0; i < num_streams; i++) {
            stream[i].width = le16_to_cpu(response->config[i].width);
            stream[i].height = le16_to_cpu(response->config[i].height);
            stream[i].format = le16_to_cpu(response->config[i].format);
            stream[i].virtual_channel = response->config[i].virtual_channel;
            stream[i].data_type = response->config[i].data_type;
            stream[i].max_size = le32_to_cpu(response->config[i].max_size);
        }
    }

    kfree(response);
    kfree(request);
	return 0;
    
err_free_resp:
    kfree(response);
err_free_req:
    kfree(request);
    return ret;
}

/**
 * @brief The capture operation
 */
static int gb_camera_capture(struct gb_camera *gb_cam, u32 request_id,
                             u8 streams, u16 num_frames, const u8 *settings,
                             u16 size)
{
    struct gb_camera_capture_request *request;
    int req_size;
	int ret;

    /* setup request */
    req_size = sizeof(*request) + size;
    
    request = kzalloc(req_size, GFP_KERNEL);
    if (!request) {
        return -ENOMEM;
    }
    
    request->request_id = cpu_to_le32(request_id);
    request->streams = streams;
    request->num_frames = cpu_to_le16(num_frames);
    memcpy(request->settings, settings, size);

    /* send out request */
	ret = gb_operation_sync(gb_cam->connection, GB_CAMERA_TYPE_CAPTURE,
                            request, req_size,
                            NULL, 0);

    if (ret) {
        goto err_free_mem;
    }

    /* todo: the code to handle capture */

    kfree(request);
    return 0;

err_free_mem:
    kfree(request);
    return ret;
}

/**
 * @brief The flush operation
 */
static int gb_camera_flush(struct gb_camera *gb_cam, u32 *request_id)
{
    struct gb_camera_flush_response response;
	int ret;
    
    /* send out request */
	ret = gb_operation_sync(gb_cam->connection, GB_CAMERA_TYPE_FLUSH,
                            NULL, 0,
                            &response, sizeof(response));
    if (ret) {
        return ret;
    }

    /* todo: the code to handle capture */

    *request_id = le32_to_cpu(response.request_id);

    return 0;
}

/**
 * @brief The metadata operation
 */
static int gb_camera_metadata(struct gb_camera *gb_cam, u32 request_id,
                              u16 frame_number, u8 stream, u16 size,
                              const u8 *metadata)
{
    struct gb_camera_metadata_request *request;
    int req_size;
	int ret;

    /* setup request */
    req_size = sizeof(*request) + size;

    request = kzalloc(req_size, GFP_KERNEL);
    if (!request) {
        return -ENOMEM;
    }
    
    request->request_id = cpu_to_le32(request_id);
    request->frame_number = cpu_to_le16(frame_number);
    request->stream = stream;
    memcpy(request->data, metadata, size);
    
    /* send out request */
	ret = gb_operation_sync(gb_cam->connection, GB_CAMERA_TYPE_METADATA,
                            request, req_size,
                            NULL, 0);
    if (ret) {
        return ret;
    }

    /* todo: the code to handle metadata */

    kfree(request);
    return 0;
}

/**
 * @brief 
 */
void test_capabilities(void)
{
    int i;
    u16 size = 16;
    u8 capab[16];
        
    gb_camera_capabilities(camera_gb, &size, capab);

    for (i = 0; i < size; i++) {
        printk("0x%2x ", capab[i]);
    }
    printk("\n");
}

/**
 * @brief 
 */
void test_configure_stream(void)
{
    struct gb_stream_config_resp stream;

    stream.width = 1920;
    stream.height = 1080;
    stream.format = 3;
    stream.virtual_channel = 0;
    stream.data_type = 0;
    stream.max_size = 0;
    
    gb_camera_configure_stream(camera_gb, 1, &stream);

    printk("width = %d \n", stream.width);
    printk("height = %d \n", stream.height);
    printk("format = %d \n", stream.format);
    printk("virtual_channel = %d \n", stream.virtual_channel);
    printk("data_type = %d \n", stream.data_type);
    printk("max_size = %d \n", stream.max_size);
}

/**
 * @brief 
 */
void test_capture(void)
{
    u8 settings[8];
    
    gb_camera_capture(camera_gb, 1234, 1, 5, settings, 8);
}

/**
 * @brief 
 */
void test_flush(void)
{
    u32 request_id = 0;
    
    gb_camera_flush(camera_gb, &request_id);

    printk("request_id = %d \n", request_id);
}

/**
 * @brief 
 */
void test_metadata(void)
{
    u8 metadata[8];
    
    gb_camera_metadata(camera_gb, 1234, 8, 3, 8, metadata);
}

/*
static ssize_t camera_read(struct file *f, char __user *buf,
				size_t count, loff_t *ppos)
{
	ssize_t ret;
    
	return ret;
}
*/

static ssize_t camera_write(struct file *f, const char __user *buf,
				size_t count, loff_t *ppos)
{
	/* ssize_t ret;*/

    switch (buf[0]) {
    case 'a':
        printk("a: capabilities() test \n");
        test_capabilities();
        break;
    case 'b':
        printk("b: configure_stream() test \n");
        test_configure_stream();
        break;
    case 'c':
        printk("c: capture() test \n");
        test_capture();
        break;
    case 'd':
        printk("d: flush() test \n");
        test_flush();
        break;
    case 'e':
        printk("e: metadata() test \n");
        test_metadata();
        break;
    default:
        printk("Not supprot command !!! \n");
    }

	return count;
}

static struct dentry *camera_dentry;

static const struct file_operations camera_fops = {
	/*.read	= camera_read,*/
    .write	= camera_write,
};

static void camera_enable(void)
{
	camera_dentry = debugfs_create_file("camera", (S_IWUSR | S_IRUGO),
						gb_debugfs_get(), NULL,
						&camera_fops);
}

/**
 * @brief Camera protocol init
 */
static int gb_camera_connection_init(struct gb_connection *connection)
{
	struct gb_camera *gb;
	int ret;

	gb = kzalloc(sizeof(*gb), GFP_KERNEL);
	if (!gb)
		return -ENOMEM;

	gb->connection = connection;
	connection->private = gb;
    camera_gb = gb;
    
    camera_enable();

	return 0;
}

/**
 * @brief Camera protocol exit
 */
static void gb_camera_connection_exit(struct gb_connection *connection)
{
	struct gb_camera *gb = connection->private;

    if (!gb) {
        return;
    }

    /* implement exit code here */
    
	kfree(gb);
}

static struct gb_protocol camera_protocol = {
	.name			= "camera",
	.id			    = GREYBUS_PROTOCOL_CAMERA,
	.major			= GB_CAMERA_VERSION_MAJOR,
	.minor			= GB_CAMERA_VERSION_MINOR,
	.connection_init	= gb_camera_connection_init,
	.connection_exit	= gb_camera_connection_exit,
	.request_recv		= NULL,	/* no incoming requests */
};

gb_builtin_protocol_driver(camera_protocol);
