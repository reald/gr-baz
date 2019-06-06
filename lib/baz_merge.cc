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

#include <baz_merge.h>
#include <gnuradio/io_signature.h>

#include <gnuradio/blocks/pdu.h>

#include <boost/format.hpp>

#include <stdio.h>

/*
 * Create a new instance of baz_pow_cc and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
baz_merge_sptr 
baz_make_merge (int item_size, float samp_rate, int additional_streams /*= 1*/, bool drop_residual /*= true*/, const char* length_tag /*= "length"*/, const char* ignore_tag /*= "ignore"*/, bool verbose /*= false*/)
{
	return baz_merge_sptr (new baz_merge (item_size, samp_rate, additional_streams, drop_residual, length_tag, ignore_tag, verbose));
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
baz_merge::baz_merge (int item_size, float samp_rate, int additional_streams, bool drop_residual, const char* length_tag, const char* ignore_tag, bool verbose = false)
	: gr::block ("merge",
		gr::io_signature::make (MIN_IN, /*MAX_IN*/MIN_IN+additional_streams, item_size),
		gr::io_signature::make (MIN_OUT, MAX_OUT, item_size))
	, d_samp_rate(samp_rate)
	, d_drop_residual(drop_residual)
	, d_verbose(verbose)
	, d_start_time_whole(0)
	, d_start_time_frac(0.0)
	, d_selected_input(0)
	, d_items_to_copy(0)
	//, d_items_to_ignore(0)
	, d_ignore_current(false)
	, d_length_name(pmt::intern(length_tag))
	, d_ignore_name(pmt::intern(ignore_tag))
	// FIXME: flush tag
	, d_total_burst_count(0)
{
	fprintf(stderr, "[%s<%li>] item size: %d, sample rate: %f, additional streams: %d: length tag: \'%s\', ignore tag: \'%s\', verbose: %s\n", name().c_str(), unique_id(), item_size, samp_rate, additional_streams, length_tag, ignore_tag, (d_verbose ? "yes" : "no"));
	
	//set_relative_rate(1);
	
	set_tag_propagation_policy(block::TPP_DONT);
	
	for (int i = 0; i < additional_streams; ++i)
	{
		pmt::pmt_t id = pmt::string_to_symbol(boost::str(boost::format("%d") % (i+1)));	// When using Any in GRC, msg port name is just number (no 'out')
		msg_output_ids.push_back(id);
		message_port_register_out(id);
	}
}

/*
 * Our virtual destructor.
 */
baz_merge::~baz_merge ()
{
}

void baz_merge::set_start_time(double time)
{
	d_start_time_whole = (uint64_t)time;
	d_start_time_frac = time - (double)d_start_time_whole;
}

void baz_merge::set_start_time(uint64_t whole, double frac)
{
	d_start_time_whole = whole;
	d_start_time_frac = frac;
}

void baz_merge::forecast(int noutput_items, gr_vector_int &ninput_items_required)
{
	ninput_items_required[0] = noutput_items;
	
	for (size_t i = 1; i < ninput_items_required.size(); ++i)
	{
		ninput_items_required[i] = 0;
	}
	
	//if (d_selected_input > 0)
	//	ninput_items_required[i] = noutput_items;
}

int baz_merge::general_work(int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items)
{
	size_t item_size = output_signature()->sizeof_stream_item(0);
	
	const char *in = (const char *) input_items[0];
	char *out = (char *) output_items[0];
	
	/*if ((ninput_items.size() > 1) && (ninput_items[1] > 0))
	{
		fprintf(stderr, "[%s<%i>] items pending: %d\n", name().c_str(), unique_id(), ninput_items[1]);
	}*/
	
	if ((d_selected_input > 0) && (d_items_to_copy > 0))	// With length tag
	{
		const uint64_t nread = nitems_read(d_selected_input);
		
		if (ninput_items[d_selected_input] > 0)
		{
			int to_copy = std::min(std::min(noutput_items, d_items_to_copy), ninput_items[d_selected_input]);
			assert(to_copy > 0);
			
			if (d_ignore_current == false)
			{
				memcpy(out, (const char *)input_items[d_selected_input], to_copy * item_size);
				
				pmt::pmt_t msg_flush = pmt::string_to_symbol("flush");
				
				pmt::pmt_t msg_dict = pmt::make_dict();
				msg_dict = dict_add(msg_dict, msg_flush, pmt::PMT_T);

				//pmt::pmt_t pdu_vector = gr::blocks::pdu::make_pdu_vector(gr::blocks::pdu::byte_t, (const uint8_t*)"", 0);
				pmt::pmt_t pdu_vector = pmt::init_u8vector(1, (const uint8_t*)"");

				pmt::pmt_t msg = pmt::cons(msg_dict, pdu_vector);
				
				message_port_pub(msg_output_ids[d_selected_input - 1], msg);
			}
			
			d_items_to_copy -= to_copy;
			assert(d_items_to_copy >= 0);
			
			consume(d_selected_input, to_copy);
			
			bool ignoring_current = d_ignore_current;
			
			if (d_items_to_copy == 0)
			{
				//fprintf(stderr, "[%s<%i>] burst %llu finished on sample %llu\n", name().c_str(), unique_id(), d_total_burst_count, (nread + to_copy - 1));
				
				d_selected_input = 0;
				d_ignore_current = false;
			}
			
			return (ignoring_current ? 0 : to_copy);	// FIXME: Check if only one flush is necessary, or if underrun occurs because flushing takes too long (output channel 0 during ignore instead)
		}
		else
		{
			if (d_ignore_current)
			{
				if (d_verbose) fprintf(stderr, "[%s<%li>] no samples for burst %llu on sample %llu\n", name().c_str(), unique_id(), d_total_burst_count, nread);
				
				d_selected_input = 0;
				d_ignore_current = false;
				d_items_to_copy = 0;
			}
			
			return 0;	// Waiting for more samples to arrive on selected input
		}
	}
	else if (d_selected_input > 0)	// Last iteration used this input
	{
		assert(false);	// FIXME: Depend on policy (exhaust selected, or always pick highest index)
	}
	
	////////////////////////////////////////////////////////////////////////////
	// Can only get here once selected input has had all items copied
	
	for (int i = (int)ninput_items.size() - 1; i > 0; i--)
	{
		if (ninput_items[i] == 0)
			continue;
		
		std::vector<gr::tag_t> tags, ignore_tags;
		const uint64_t nread = nitems_read(i);
		
		get_tags_in_range(tags, i, nread, nread + ninput_items[i], d_length_name);
		//std::sort(tags.begin(), tags.end(), tag_t::offset_compare);
		
		get_tags_in_range(ignore_tags, i, nread, nread + ninput_items[i], d_ignore_name);
		//std::sort(ignore_tags.begin(), ignore_tags.end(), tag_t::offset_compare);
		
		if (tags.size() > 0)
		{
			gr::tag_t& tag = tags[0];
			if (tag.offset != nread)	// First tag is further along in sample stream
			{
				assert(tag.offset > nread);
				
				uint64_t diff = tag.offset - nread;
				
				if (d_drop_residual)
				{
					consume(i, diff);
					
					//return 0;
					continue;	// Continue with loop
				}
				else	// This form of copy does not enforce selection lock to this stream!
				{
					int to_copy = std::min((int)diff, noutput_items);
					
					memcpy(out, (const char *)input_items[i], to_copy * item_size);
					
					consume(i, to_copy);
					
					return to_copy;
				}
			}
			else	// First tag is on first sample
			{
				++d_total_burst_count;
				
				d_selected_input = i;
				d_items_to_copy = pmt::to_long(tag.value);
				
				BOOST_FOREACH(gr::tag_t& ignore_tag, ignore_tags)
				{
					if (ignore_tag.offset != nread)
					{
						if (d_verbose) fprintf(stderr, "! Burst #%llu: Ignoring 'ignore' tag at %llu (expecting %llu)\n", d_total_burst_count, ignore_tag.offset, nread);
						continue;
					}
					
					d_ignore_current = true;
					
					break;
				}
				
				if (d_verbose) fprintf(stderr, "[%s<%li>] beginning burst %llu of length %d at sample %llu on input %d (ignoring: %s)\n", name().c_str(), unique_id(), d_total_burst_count, d_items_to_copy, nread, d_selected_input, (d_ignore_current ? "yes" : "no"));
				
				return 0;
			}
		}
		else	// There are no tags
		{
			if (d_drop_residual)
			{
				consume(i, ninput_items[i]);
				
				//return 0;
				continue;	// Continue with loop
			}
			else	// This form of copy does not enforce selection lock to this stream!
			{
				int to_copy = std::min(ninput_items[i], noutput_items);
				
				memcpy(out, (const char *)input_items[i], to_copy * item_size);
				
				consume(i, to_copy);
				
				return to_copy;
			}
		}
	}
	
	////////////////////////////////////////////////////////////////////////////
	// Nothing available, normal copy
	
	int to_copy = std::min(ninput_items[0], noutput_items);
	//assert(ninput_items[0] == noutput_items);	// ?
	
	memcpy(out, in, to_copy * item_size);
	
	consume(0, to_copy);
	
	return to_copy;
}
