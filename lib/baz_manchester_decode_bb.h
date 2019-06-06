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

#ifndef INCLUDED_BAZ_MANCHESTER_DECODE_BB_H
#define INCLUDED_BAZ_MANCHESTER_DECODE_BB_H

#include <gnuradio/sync_block.h>
#include <deque>

class BAZ_API baz_manchester_decode_bb;

/*
 * We use boost::shared_ptr's instead of raw pointers for all access
 * to gr::blocks (and many other data structures).  The shared_ptr gets
 * us transparent reference counting, which greatly simplifies storage
 * management issues.  This is especially helpful in our hybrid
 * C++ / Python system.
 *
 * See http://www.boost.org/libs/smart_ptr/smart_ptr.htm
 *
 * As a convention, the _sptr suffix indicates a boost::shared_ptr
 */
typedef boost::shared_ptr<baz_manchester_decode_bb> baz_manchester_decode_bb_sptr;

/*!
 * \brief Return a shared_ptr to a new instance of baz_manchester_decode_bb.
 *
 * To avoid accidental use of raw pointers, baz_manchester_decode_bb's
 * constructor is private.  howto_make_square2_ff is the public
 * interface for creating new instances.
 */
BAZ_API baz_manchester_decode_bb_sptr baz_make_manchester_decode_bb (bool original, int threshold, int window, bool verbose = false, bool show_bits = false);

/*!
 * \brief square2 a stream of floats.
 * \ingroup block
 *
 * This uses the preferred technique: subclassing gr::sync_block.
 */
class BAZ_API baz_manchester_decode_bb : public gr::block
{
private:
  // The friend declaration allows howto_make_square2_ff to
  // access the private constructor.

  friend BAZ_API baz_manchester_decode_bb_sptr baz_make_manchester_decode_bb (bool original, int threshold, int window, bool verbose, bool show_bits);

  baz_manchester_decode_bb (bool original, int threshold, int window, bool verbose, bool show_bits);  	// private constructor
  
  bool d_original, d_verbose, d_show_bits;
  int d_threshold, d_window;
  int d_current_window, d_violation_count;
  int d_offset;
  std::deque<bool> d_violation_history;	// FIXME: Bit sequence
  int d_violation_total_count;

 public:
  ~baz_manchester_decode_bb ();	// public destructor

  //void set_exponent(float exponent);
  
  //inline float exponent() const
  //{ return d_exponent; }

  void forecast(int noutput_items, gr_vector_int &ninput_items_required);
  int general_work (int noutput_items, gr_vector_int &ninput_items, gr_vector_const_void_star &input_items, gr_vector_void_star &output_items);
};

#endif /* INCLUDED_BAZ_MANCHESTER_DECODE_BB_H */
