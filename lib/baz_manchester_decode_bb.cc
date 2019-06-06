/* -*- c++ -*- */
/*
 * Copyright 2004 Free Software Foundation, Inc.
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

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <baz_manchester_decode_bb.h>
#include <gnuradio/io_signature.h>
//#include <volk/volk.h>

#include <stdio.h>

/*
 * Create a new instance of baz_manchester_decode_bb and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
baz_manchester_decode_bb_sptr 
baz_make_manchester_decode_bb (bool original, int threshold, int window, bool verbose /*= false*/, bool show_bits /*= false*/)
{
  return baz_manchester_decode_bb_sptr (new baz_manchester_decode_bb (original, threshold, window, verbose, show_bits));
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr::block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;	// mininum number of input streams
static const int MAX_IN = 1;	// maximum number of input streams
static const int MIN_OUT = 1;	// minimum number of output streams
static const int MAX_OUT = 1;	// maximum number of output streams

/*
 * The private constructor
 */
baz_manchester_decode_bb::baz_manchester_decode_bb (bool original, int threshold, int window, bool verbose, bool show_bits)
  : gr::block ("manchester_decode_bb",
		   gr::io_signature::make (MIN_IN, MAX_IN, sizeof (char)),
		   gr::io_signature::make (MIN_OUT, MAX_OUT, sizeof (char)))
  , d_original(original), d_threshold(threshold), d_window(window), d_verbose(verbose), d_show_bits(show_bits)
  , d_current_window(0), d_violation_count(0), d_offset(0), d_violation_total_count(0)
{
	fprintf(stderr, "[%s<%li>] original: %s, threshold: %d, window: %d\n", name().c_str(), unique_id(), (original ? "yes" : "no"), threshold, window);
	
	set_history(1+1);
	set_relative_rate(0.5);
}

/*
 * Our virtual destructor.
 */
baz_manchester_decode_bb::~baz_manchester_decode_bb ()
{
}
/*
void baz_manchester_decode_bb::set_exponent(float exponent)
{
  d_exponent = exponent;
}
*/
void baz_manchester_decode_bb::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
	for (size_t i = 0; i < ninput_items_required.size(); ++i)
		ninput_items_required[i] = noutput_items * 2;
}

int baz_manchester_decode_bb::general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	const char *in = (const char *) input_items[0];
	char *out = (char *) output_items[0];

	bool skip = false;
	int noutput = 0;

	int i;
	for (i = d_offset; i < noutput_items; i+=2)
	{
		if ((i + 1) == noutput_items)	// Next iteration last bit will be at index 0
			break;
		
		assert((i + 1) < noutput_items);
		
		bool first = in[i];
		bool second = in[i + 1];
		
		if (d_current_window < d_window)
			++d_current_window;
		
		if (d_violation_history.size() == d_window)
			d_violation_history.pop_front();
		
		if (first == second)
		{
			++d_violation_count;
			
			d_violation_history.push_back(true);
			
			if (d_show_bits)
			{
				//fprintf(stderr, "[%s<%i>] violation (%d %d)\n", name().c_str(), unique_id(), (int)first, (int)second);
				fprintf(stderr, " ! ");
				fflush(stderr);
			}
		}
		else
		{
			d_violation_history.push_back(false);
			
			bool bit = ((first == false) && (second == true));
			bit = (d_original ? !bit : bit);
			
			out[noutput++] = (bit ? 0x01 : 0x00);
			
			if (d_show_bits)
			{
				fprintf(stderr, "%d", (int)bit);
				fflush(stderr);
			}
		}
		
		if (d_violation_history.size() == d_window)
		{
			int violation_count = 0;
			for (size_t n = 0; n < d_violation_history.size(); n++)
			{
				if (d_violation_history[n])	// FIXME: Optimise later: circular buffer, pre-populate with 0s, maintain running count and update when popping earliest value
					++violation_count;
			}
			
			if (violation_count >= d_threshold)
			{
				++d_violation_total_count;

				d_violation_history.clear();
				
				--i;	// Rewind and re-use previous this bit as first of next pair
				
				if (d_verbose)
				{
					if (d_show_bits)
						fprintf(stderr, "\n");
					fprintf(stderr, "[%s<%li>] violation threshold exceeded (# %d)\n", name().c_str(), unique_id(), d_violation_total_count);
				}
			}
		}
		
		/*uint64_t wrong_bits = 0;
		uint64_t nwrong = d_threshold+1;
		wrong_bits  = (d_data_reg ^ d_access_code) & d_mask;
		volk_64u_popcnt(&nwrong, wrong_bits);*/
	}
	
	consume(0, i);

	return noutput;
}
