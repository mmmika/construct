// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SIMD_IOSTREAM_H

namespace ircd::simd
{
	template<class block_t>
	using iostream_fixed_proto = void (block_t &, block_t mask);

	template<class block_t>
	using iostream_variable_proto = u64x2 (block_t &, block_t mask);

	template<class block_t,
	         class lambda>
	using iostream_is_fixed_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t &, block_t>, void
	>;

	template<class block_t,
	         class lambda>
	using iostream_is_variable_stride = std::is_same
	<
		std::invoke_result_t<lambda, block_t &, block_t>, u64x2
	>;

	template<class block_t,
	         class lambda>
	using iostream_fixed_stride = std::enable_if
	<
		iostream_is_fixed_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	using iostream_variable_stride = std::enable_if
	<
		iostream_is_variable_stride<block_t, lambda>::value, u64x2
	>;

	template<class block_t,
	         class lambda>
	typename iostream_fixed_stride<block_t, lambda>::type
	stream(char *, const char *, const u64x2, lambda&&) noexcept;

	template<class block_t,
	         class lambda>
	typename iostream_variable_stride<block_t, lambda>::type
	stream(char *, const char *, const u64x2, lambda&&) noexcept;
}

/// Streaming transform
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the input and output buffers do not have to
/// be aligned and can be any size.
///
/// * full-duplex: the operation involves both input and output and there are
/// separate pointers for progress across the input and output buffers which
/// are incremented independently.
///
/// * variable-stride: progress for each iteration of the loop across the input
/// and output buffers is not fixed; the transform function may advance either
/// pointer zero to sizeof(block_t) bytes each iteration. Due to these
/// characteristics, unaligned bytes may be redundantly loaded or stored and
/// non-temporal features are not used to optimize the operation.
///
/// u64x2 counter lanes = { output_length, input_length }; The argument `max`
/// gives the buffer size in that format. The return value is the consumed
/// bytes (final counter value) in that format.
///
template<class block_t,
         class lambda>
inline typename ircd::simd::iostream_variable_stride<block_t, lambda>::type
ircd::simd::stream(char *const __restrict__ out,
                   const char *const __restrict__ in,
                   const u64x2 max,
                   lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		0, // output pos
		0, // input pos
	};

	// primary broadband loop
	while(count[1] + sizeof(block_t) <= max[1] && count[0] + sizeof(block_t) <= max[0])
	{
		static const auto mask
		{
			~block_t{0}
		};

		const auto di
		{
			reinterpret_cast<block_t_u *>(out + count[0])
		};

		const auto si
		{
			reinterpret_cast<const block_t_u *>(in + count[1])
		};

		block_t block
		(
			*si
		);

		const auto consume
		{
			closure(block, mask)
		};

		count += consume;
		*di = block;
	}

	// trailing narrowband loop
	while(count[1] < max[1])
	{
		block_t block {0}, mask {0};
		for(size_t i(0); count[1] + i < max[1] && i < sizeof(block_t); ++i)
		{
			block[i] = in[count[1] + i];
			mask[i] = 0xff;
		}

		const auto consume
		{
			closure(block, mask)
		};

		for(size_t i(0); i < consume[0] && count[0] + i < max[0]; ++i)
			out[count[0] + i] = block[i];

		count += consume;
	}

	return u64x2
	{
		std::min(count[0], max[0]),
		std::min(count[1], max[1]),
	};
}

/// Streaming transform
///
/// This template performs the loop boiler-plate for the developer who can
/// simply supply a conforming closure. Characteristics:
///
/// * byte-aligned (unaligned): the input and output buffers do not have to
/// be aligned and can be any size.
///
/// * full-duplex: the operation involves both input and output and there are
/// separate pointers for progress across the input and output buffers which
/// are incremented independently.
///
/// * fixed-stride: progress for each iteration of the loop across the input
/// and output buffers is fixed.
///
/// u64x2 counter lanes = { output_length, input_length }; The argument `max`
/// gives the buffer size in that format. The return value is the consumed
/// bytes (final counter value) in that format.
///
template<class block_t,
         class lambda>
inline typename ircd::simd::iostream_fixed_stride<block_t, lambda>::type
ircd::simd::stream(char *const __restrict__ out,
                   const char *const __restrict__ in,
                   const u64x2 max,
                   lambda&& closure)
noexcept
{
	using block_t_u = unaligned<block_t>;

	u64x2 count
	{
		0, // output pos
		0, // input pos
	};

	// primary broadband loop
	while(count[1] + sizeof(block_t) <= max[1] && count[0] + sizeof(block_t) <= max[0])
	{
		static const u64x2 consume
		{
			sizeof(block_t),
			sizeof(block_t),
		};

		static const auto mask
		{
			~block_t{0}
		};

		const auto di
		{
			reinterpret_cast<block_t_u *>(out + count[0])
		};

		const auto si
		{
			reinterpret_cast<const block_t_u *>(in + count[1])
		};

		block_t block
		(
			*si
		);

		closure(block, mask);
		count += consume;
		*di = block;
	}

	// trailing narrowband loop
	assert(count[1] + sizeof(block_t) > max[1]);
	if(likely(count[1] < max[1]))
	{
		u64 i[2] {0};
		block_t block {0}, mask {0};
		for(; count[1] + i[1] < max[1]; ++i[1])
		{
			block[i[1]] = in[count[1] + i[1]];
			mask[i[1]] = 0xff;
		}

		closure(block, mask);
		for(; i[0] < i[1] && count[0] + i[0] < max[0]; ++i[0])
			out[count[0] + i[0]] = block[i[0]];

		count += u64x2
		{
			i[0], i[1]
		};
	}

	assert(count[0] == max[0]);
	assert(count[1] == max[1]);
	return count;
}