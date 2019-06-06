/* -*- c++ -*- */
/*
 * Copyright 2007 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * gr-baz by Balint Seeber (http://spench.net/contact)
 * Information, documentation & samples: http://wiki.spench.net/wiki/gr-baz
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <baz_delay.h>
#include <gnuradio/io_signature.h>

#include <string.h>
#include <math.h>
#include <stdio.h>

using namespace std;

baz_delay_sptr baz_make_delay (size_t itemsize, int delay)
{
	return baz_delay_sptr (new baz_delay (itemsize, delay));
}

baz_delay::baz_delay (size_t itemsize, int delay)
  : gr::block ("variable_delay",
		gr::io_signature::make (1, 1, itemsize),
		gr::io_signature::make (1, 1, itemsize))
	, d_itemsize(itemsize)
	, d_delay(0)
	, d_new_delay(0)
	, d_update(false)
{
	fprintf(stderr, "[%s<%li>] item size: %lu, delay: %d\n", name().c_str(), unique_id(), itemsize, delay);

	// Anything greater than this will cause the scheduler to stall (default max is 64k)
	// FIXME: Limit arg, or default to 2x
	//set_min_output_buffer(delay);

	set_delay(delay);
}

void baz_delay::set_delay(int delay)	// +ve: past, -ve: future
{
	boost::mutex::scoped_lock guard(d_mutex);

	//fprintf(stderr, "[%s<%i>] delay: %d (was: %d)\n", name().c_str(), unique_id(), delay, d_delay);

	//d_delay = delay;
	d_new_delay = delay;
	d_update = true;
}

void baz_delay::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
	//boost::mutex::scoped_lock guard(d_mutex);
	
	const int64_t diff = ((int64_t)nitems_written(0) - (int64_t)nitems_read(0)) - d_delay;
	
	//if (diff != 0)
	//	fprintf(stderr, "[%s<%i>] forecast diff: %d (noutput_items: %d)\n", name().c_str(), unique_id(), diff, noutput_items);
	
	for (size_t i = 0; i < ninput_items_required.size(); ++i)
	{
		//ninput_items_required[i] = ((diff >= 0) ? noutput_items : 0);

		if (diff < 0)
			ninput_items_required[i] = 0;
		else
			ninput_items_required[i] = noutput_items;
	}
}

int baz_delay::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	//boost::mutex::scoped_lock guard(d_mutex);
	
	const int64_t diff = ((int64_t)nitems_written(0) - (int64_t)nitems_read(0)) - d_delay;

	int res = 0;
	
	if (diff < 0)	// -ve: past
	{
		/*if (nitems_read(0) == 0)
		{
			int64_t to_copy = std::min(-diff, (int64_t)noutput_items);
			fprintf(stderr, "[%s<%i>] diff: %lld, zeroes to copy: %lld\n", name().c_str(), unique_id(), diff, to_copy);
			memset(output_items[0], 0x00, d_itemsize * to_copy);
			return to_copy;
		}*/

		//int64_t to_copy = std::min(-diff, (int64_t)ninput_items[0]);
		//int64_t to_copy = std::min(std::min(-diff, (int64_t)ninput_items[0]), (int64_t)noutput_items);
		int64_t to_copy = std::min(-diff, (int64_t)noutput_items);
		if (ninput_items[0] == 0)
		{
			//fprintf(stderr, "[%s<%i>] No input items!\n", name().c_str(), unique_id());

			memset(output_items[0], 0x00, d_itemsize * to_copy);
		}
		else
		{
			for (int64_t i = 0; i < to_copy; ++i)
				memcpy((char*)output_items[0] + (d_itemsize * i), input_items[0], d_itemsize);
		}
		res = to_copy;
	}
	else if (diff > 0)	// +ve: future
	{
		int64_t to_consume = std::min(diff, (int64_t)ninput_items[0]);
		consume(0, to_consume);
		res = 0;
	}
	else
	{
		memcpy(output_items[0], input_items[0], (d_itemsize * noutput_items));
	
		consume(0, noutput_items);

		res = noutput_items;
	}

	boost::mutex::scoped_lock guard(d_mutex);

	if (d_update)
	{
		d_delay = d_new_delay;
		d_update = false;
	}

	return res;
}
