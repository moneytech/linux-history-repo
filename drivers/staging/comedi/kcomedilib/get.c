/*
    kcomedilib/get.c
    a comedlib interface for kernel modules

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 1997-2000 David A. Schleef <ds@schleef.org>

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

#define __NO_VERSION__
#include "../comedi.h"
#include "../comedilib.h"
#include "../comedidev.h"

int comedi_get_n_subdevices(comedi_t * d)
{
	comedi_device *dev = (comedi_device *) d;

	return dev->n_subdevices;
}

int comedi_get_version_code(comedi_t * d)
{
	return COMEDI_VERSION_CODE;
}

const char *comedi_get_driver_name(comedi_t * d)
{
	comedi_device *dev = (comedi_device *) d;

	return dev->driver->driver_name;
}

const char *comedi_get_board_name(comedi_t * d)
{
	comedi_device *dev = (comedi_device *) d;

	return dev->board_name;
}

int comedi_get_subdevice_type(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;

	return s->type;
}

unsigned int comedi_get_subdevice_flags(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;

	return s->subdev_flags;
}

int comedi_find_subdevice_by_type(comedi_t * d, int type, unsigned int subd)
{
	comedi_device *dev = (comedi_device *) d;

	if (subd > dev->n_subdevices)
		return -ENODEV;

	for (; subd < dev->n_subdevices; subd++) {
		if (dev->subdevices[subd].type == type)
			return subd;
	}
	return -1;
}

int comedi_get_n_channels(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;

	return s->n_chan;
}

int comedi_get_len_chanlist(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;

	return s->len_chanlist;
}

lsampl_t comedi_get_maxdata(comedi_t * d, unsigned int subdevice,
	unsigned int chan)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;

	if (s->maxdata_list)
		return s->maxdata_list[chan];

	return s->maxdata;
}

#ifdef KCOMEDILIB_DEPRECATED
int comedi_get_rangetype(comedi_t * d, unsigned int subdevice,
	unsigned int chan)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	int ret;

	if (s->range_table_list) {
		ret = s->range_table_list[chan]->length;
	} else {
		ret = s->range_table->length;
	}

	ret = ret | (dev->minor << 28) | (subdevice << 24) | (chan << 16);

	return ret;
}
#endif

int comedi_get_n_ranges(comedi_t * d, unsigned int subdevice, unsigned int chan)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	int ret;

	if (s->range_table_list) {
		ret = s->range_table_list[chan]->length;
	} else {
		ret = s->range_table->length;
	}

	return ret;
}

/*
 * ALPHA (non-portable)
*/
int comedi_get_krange(comedi_t * d, unsigned int subdevice, unsigned int chan,
	unsigned int range, comedi_krange * krange)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	const comedi_lrange *lr;

	if (s->range_table_list) {
		lr = s->range_table_list[chan];
	} else {
		lr = s->range_table;
	}
	if (range >= lr->length) {
		return -EINVAL;
	}
	memcpy(krange, lr->range + range, sizeof(comedi_krange));

	return 0;
}

/*
 * ALPHA (may be renamed)
*/
unsigned int comedi_get_buf_head_pos(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;

	async = s->async;
	if (async == NULL)
		return 0;

	return async->buf_write_count;
}

int comedi_get_buffer_contents(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;
	unsigned int num_bytes;

	if (subdevice >= dev->n_subdevices)
		return -1;
	async = s->async;
	if (async == NULL)
		return 0;
	num_bytes = comedi_buf_read_n_available(s->async);
	return num_bytes;
}

/*
 * ALPHA
*/
int comedi_set_user_int_count(comedi_t * d, unsigned int subdevice,
	unsigned int buf_user_count)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;
	int num_bytes;

	async = s->async;
	if (async == NULL)
		return -1;

	num_bytes = buf_user_count - async->buf_read_count;
	if (num_bytes < 0)
		return -1;
	comedi_buf_read_alloc(async, num_bytes);
	comedi_buf_read_free(async, num_bytes);

	return 0;
}

int comedi_mark_buffer_read(comedi_t * d, unsigned int subdevice,
	unsigned int num_bytes)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;

	if (subdevice >= dev->n_subdevices)
		return -1;
	async = s->async;
	if (async == NULL)
		return -1;

	comedi_buf_read_alloc(async, num_bytes);
	comedi_buf_read_free(async, num_bytes);

	return 0;
}

int comedi_mark_buffer_written(comedi_t * d, unsigned int subdevice,
	unsigned int num_bytes)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;
	int bytes_written;

	if (subdevice >= dev->n_subdevices)
		return -1;
	async = s->async;
	if (async == NULL)
		return -1;
	bytes_written = comedi_buf_write_alloc(async, num_bytes);
	comedi_buf_write_free(async, bytes_written);
	if (bytes_written != num_bytes)
		return -1;
	return 0;
}

int comedi_get_buffer_size(comedi_t * d, unsigned int subdev)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdev;
	comedi_async *async;

	if (subdev >= dev->n_subdevices)
		return -1;
	async = s->async;
	if (async == NULL)
		return 0;

	return async->prealloc_bufsz;
}

int comedi_get_buffer_offset(comedi_t * d, unsigned int subdevice)
{
	comedi_device *dev = (comedi_device *) d;
	comedi_subdevice *s = dev->subdevices + subdevice;
	comedi_async *async;

	if (subdevice >= dev->n_subdevices)
		return -1;
	async = s->async;
	if (async == NULL)
		return 0;

	return async->buf_read_ptr;
}
