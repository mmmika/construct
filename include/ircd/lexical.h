/*
 *  charybdis: an advanced ircd.
 *  inline/stringops.h: inlined string operations used in a few places
 *
 *  Copyright (C) 2005-2016 Charybdis Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

#pragma once
#define HAVE_IRCD_LEXICAL_H

//
// Lexical conversions
//
namespace ircd
{
	IRCD_EXCEPTION(ircd::error, bad_lex_cast)

	template<class T> bool try_lex_cast(const string_view &);
	template<> bool try_lex_cast<std::string>(const string_view &);       // stub always true
	template<> bool try_lex_cast<std::string_view>(const string_view &);  // stub always true
	template<> bool try_lex_cast<string_view>(const string_view &);       // stub always true
	template<> bool try_lex_cast<long double>(const string_view &);
	template<> bool try_lex_cast<double>(const string_view &);
	template<> bool try_lex_cast<ulong>(const string_view &);
	template<> bool try_lex_cast<long>(const string_view &);
	template<> bool try_lex_cast<uint>(const string_view &);
	template<> bool try_lex_cast<int>(const string_view &);
	template<> bool try_lex_cast<ushort>(const string_view &);
	template<> bool try_lex_cast<short>(const string_view &);
	template<> bool try_lex_cast<uint8_t>(const string_view &);
	template<> bool try_lex_cast<int8_t>(const string_view &);
	template<> bool try_lex_cast<bool>(const string_view &);

	template<class T> T lex_cast(std::string &);
	template<class T> T lex_cast(const std::string &);
	template<class T> T lex_cast(const std::string_view &);
	template<class T> T lex_cast(const string_view &);
	template<> std::string &lex_cast(std::string &);                          // trivial
	template<> std::string lex_cast(const std::string &);                     // trivial
	template<> std::string_view lex_cast(const std::string_view &);           // trivial
	template<> std::string lex_cast(const string_view &);                     // trivial
	template<> long double lex_cast(const string_view &);
	template<> double lex_cast(const string_view &);
	template<> ulong lex_cast(const string_view &);
	template<> long lex_cast(const string_view &);
	template<> uint lex_cast(const string_view &);
	template<> int lex_cast(const string_view &);
	template<> ushort lex_cast(const string_view &);
	template<> short lex_cast(const string_view &);
	template<> uint8_t lex_cast(const string_view &);
	template<> int8_t lex_cast(const string_view &);
	template<> bool lex_cast(const string_view &);

	// User supplied destination buffer
	template<class T> string_view lex_cast(T, char *const &buf, const size_t &max);
	template<> string_view lex_cast(const std::string &, char *const &buf, const size_t &max);
	template<> string_view lex_cast(const std::string_view &, char *const &buf, const size_t &max);
	template<> string_view lex_cast(const string_view &, char *const &buf, const size_t &max);
	template<> string_view lex_cast(long double, char *const &buf, const size_t &max);
	template<> string_view lex_cast(double, char *const &buf, const size_t &max);
	template<> string_view lex_cast(ulong, char *const &buf, const size_t &max);
	template<> string_view lex_cast(long, char *const &buf, const size_t &max);
	template<> string_view lex_cast(uint, char *const &buf, const size_t &max);
	template<> string_view lex_cast(int, char *const &buf, const size_t &max);
	template<> string_view lex_cast(ushort, char *const &buf, const size_t &max);
	template<> string_view lex_cast(short, char *const &buf, const size_t &max);
	template<> string_view lex_cast(uint8_t, char *const &buf, const size_t &max);
	template<> string_view lex_cast(int8_t, char *const &buf, const size_t &max);
	template<> string_view lex_cast(bool, char *const &buf, const size_t &max);

	// Circular static thread_local buffer
	const size_t LEX_CAST_BUFS {256}; // plenty
	template<class T> string_view lex_cast(const T &t);

	//
	// String tokenization.
	//

	// Use the closure for best performance. Note that string_view's
	// are not required to be null terminated. Construct an std::string from the view to allocate
	// and copy the token with null termination.
	using token_view = std::function<void (const string_view &)>;
	void tokens(const string_view &str, const char *const &sep, const token_view &);
	size_t tokens(const string_view &str, const char *const &sep, const size_t &limit, const token_view &);

	// Copies tokens into your buffer and null terminates strtok() style. Returns BYTES of buf consumed.
	size_t tokens(const string_view &str, const char *const &sep, char *const &buf, const size_t &max, const token_view &);

	// Receive token view into iterator range
	template<class it> it tokens(const string_view &str, const char *const &sep, const it &b, const it &e);

	// Receive token view into array
	template<size_t N> size_t tokens(const string_view &str, const char *const &sep, string_view (&buf)[N]);
	template<size_t N> size_t tokens(const string_view &str, const char *const &sep, std::array<string_view, N> &);

	// Receive token view into new container (custom allocator)
	template<template<class, class>
	         class C = std::vector,
	         class T = string_view,
	         class A>
	C<T, A> tokens(A allocator, const string_view &str, const char *const &sep);

	// Receive token view into new container
	template<template<class, class>
	         class C = std::vector,
	         class T = string_view,
	         class A = std::allocator<T>>
	C<T, A> tokens(const string_view &str, const char *const &sep);

	// Receive token view into new associative container (custom allocator)
	template<template<class, class, class>
	         class C,
	         class T = string_view,
	         class Comp = std::less<T>,
	         class A>
	C<T, Comp, A> tokens(A allocator, const string_view &str, const char *const &sep);

	// Receive token view into new associative container
	template<template<class, class, class>
	         class C,
	         class T = string_view,
	         class Comp = std::less<T>,
	         class A = std::allocator<T>>
	C<T, Comp, A> tokens(const string_view &str, const char *const &sep);

	// Convenience to get individual tokens
	size_t tokens_count(const string_view &str, const char *const &sep);
	string_view token(const string_view &str, const char *const &sep, const size_t &at);
	string_view token_last(const string_view &str, const char *const &sep);
	string_view token_first(const string_view &str, const char *const &sep);
	string_view tokens_after(const string_view &str, const char *const &sep, const size_t &at);

	//
	// Misc utils
	//

	// Simple case insensitive comparison convenience utils
	struct iless;
	struct igreater;
	struct iequals;

	// Vintage
	size_t strlcpy(char *const &dest, const char *const &src, const size_t &bufmax);
	size_t strlcat(char *const &dest, const char *const &src, const size_t &bufmax);
	size_t strlcpy(char *const &dest, const string_view &src, const size_t &bufmax);
	size_t strlcat(char *const &dest, const string_view &src, const size_t &bufmax);

	// Legacy
	char *strip_colour(char *string);
	char *strip_unprintable(char *string);
	char *reconstruct_parv(int parc, const char **parv);

	char chop(string_view &str);
	size_t chomp(string_view &str, const char &c = '\n');
	size_t chomp(string_view &str, const string_view &c);
	template<class T, class delim> size_t chomp(iterators<T>, const delim &d);
	string_view rstrip(const string_view &str, const char &c = ' ');
	string_view rstrip(const string_view &str, const string_view &c);
	string_view lstrip(const string_view &str, const char &c = ' ');
	string_view lstrip(const string_view &str, const string_view &c);
	string_view strip(const string_view &str, const char &c = ' ');
	string_view strip(const string_view &str, const string_view &c);
	std::pair<string_view, string_view> split(const string_view &str, const char &delim = ' ');
	std::pair<string_view, string_view> split(const string_view &str, const string_view &delim);
	std::pair<string_view, string_view> rsplit(const string_view &str, const char &delim = ' ');
	std::pair<string_view, string_view> rsplit(const string_view &str, const string_view &delim);
	string_view between(const string_view &str, const string_view &a, const string_view &b);
	string_view between(const string_view &str, const char &a = '(', const char &b = ')');
	bool endswith(const string_view &str, const string_view &val);
	bool endswith(const string_view &str, const char &val);
	template<class It> bool endswith_any(const string_view &str, const It &begin, const It &end);
	bool startswith(const string_view &str, const string_view &val);
	bool startswith(const string_view &str, const char &val);
	string_view unquote(string_view str);
	std::string unquote(std::string &&);
}

inline std::string
ircd::unquote(std::string &&str)
{
	if(endswith(str, '"'))
		str.pop_back();

	if(startswith(str, '"'))
		str = str.substr(1);

	return std::move(str);
}

inline ircd::string_view
ircd::unquote(string_view str)
{
	if(startswith(str, '"'))
		str = { str.data() + 1, str.data() + str.size() };

	if(endswith(str, '"'))
		str = { str.data(), str.data() + str.size() - 1 };

	return str;
}

inline bool
ircd::startswith(const string_view &str,
                 const char &val)
{
	return !str.empty() && str[0] == val;
}

inline bool
ircd::startswith(const string_view &str,
                 const string_view &val)
{
	const auto pos(str.find(val, 0));
	return pos == 0;
}

template<class It>
bool
ircd::endswith_any(const string_view &str,
                   const It &begin,
                   const It &end)
{
	return std::any_of(begin, end, [&str](const auto &val)
	{
		return endswith(str, val);
	});
}

inline bool
ircd::endswith(const string_view &str,
               const char &val)
{
	return !str.empty() && str[str.size()-1] == val;
}

inline bool
ircd::endswith(const string_view &str,
               const string_view &val)
{
	const auto vlen(std::min(str.size(), val.size()));
	const auto pos(str.find(val, vlen));
	return pos == str.size() - vlen;
}

inline ircd::string_view
ircd::between(const string_view &str,
              const string_view &a,
              const string_view &b)
{
	return split(split(str, a).second, b).first;
}

inline ircd::string_view
ircd::between(const string_view &str,
              const char &a,
              const char &b)
{
	return split(split(str, a).second, b).first;
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const string_view &delim)
{
	const auto pos(str.find_last_of(delim));
	if(pos == string_view::npos) return
	{
		string_view{},
		str
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + delim.size())
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::rsplit(const string_view &str,
             const char &delim)
{
	const auto pos(str.find_last_of(delim));
	if(pos == string_view::npos) return
	{
		string_view{},
		str
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + 1)
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const string_view &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str,
		string_view{}
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + delim.size())
	};
}

inline std::pair<ircd::string_view, ircd::string_view>
ircd::split(const string_view &str,
            const char &delim)
{
	const auto pos(str.find(delim));
	if(pos == string_view::npos) return
	{
		str,
		string_view{}
	};
	else return
	{
		str.substr(0, pos),
		str.substr(pos + 1)
	};
}

inline ircd::string_view
ircd::strip(const string_view &str,
            const string_view &c)
{
	return lstrip(rstrip(str, c), c);
}

inline ircd::string_view
ircd::strip(const string_view &str,
            const char &c)
{
	return lstrip(rstrip(str, c), c);
}

inline ircd::string_view
ircd::rstrip(const string_view &str,
             const string_view &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos != string_view::npos? string_view{str.substr(0, pos + 1)} : str;
}

inline ircd::string_view
ircd::rstrip(const string_view &str,
             const char &c)
{
	const auto pos(str.find_last_not_of(c));
	return pos != string_view::npos? string_view{str.substr(0, pos + 1)} : str;
}

inline ircd::string_view
ircd::lstrip(const string_view &str,
             const char &c)
{
	const auto pos(str.find_first_not_of(c));
	return pos != string_view::npos? string_view{str.substr(pos)} : string_view{};
}

inline ircd::string_view
ircd::lstrip(const string_view &str,
             const string_view &c)
{
	const auto pos(str.find_first_not_of(c));
	return pos != string_view::npos? string_view{str.substr(pos)} : string_view{};
}

template<class T,
         class delim>
size_t
ircd::chomp(iterators<T> its,
            const delim &d)
{
	return std::accumulate(begin(its), end(its), size_t(0), [&d]
	(auto ret, const auto &s)
	{
		return ret += chomp(s, d);
	});
}

inline size_t
ircd::chomp(string_view &str,
            const char &c)
{
	const auto pos(str.find_last_of(c));
	if(pos == string_view::npos)
		return 0;

	assert(str.size() - pos == 1);
	str = str.substr(0, pos);
	return 1;
}

inline size_t
ircd::chomp(string_view &str,
            const string_view &c)
{
	const auto pos(str.find_last_of(c));
	if(pos == string_view::npos)
		return 0;

	assert(str.size() - pos == c.size());
	str = str.substr(0, pos);
	return c.size();
}

inline char
ircd::chop(string_view &str)
{
	return !str.empty()? str.pop_back() : '\0';
}

template<size_t N>
size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             string_view (&buf)[N])
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<size_t N>
size_t
ircd::tokens(const string_view &str,
             const char *const &sep,
             std::array<string_view, N> &buf)
{
	const auto e(tokens(str, sep, begin(buf), end(buf)));
	return std::distance(begin(buf), e);
}

template<class it>
it
ircd::tokens(const string_view &str,
             const char *const &sep,
             const it &b,
             const it &e)
{
	it pos(b);
	tokens(str, sep, std::distance(b, e), [&pos]
	(const string_view &token)
	{
		*pos = token;
		++pos;
	});

	return pos;
}

template<template<class, class, class>
         class C,
         class T,
         class Comp,
         class A>
C<T, Comp, A>
ircd::tokens(const string_view &str,
             const char *const &sep)
{
	A allocator;
	return tokens<C, T, Comp, A>(allocator, str, sep);
}

template<template<class, class, class>
         class C,
         class T,
         class Comp,
         class A>
C<T, Comp, A>
ircd::tokens(A allocator,
             const string_view &str,
             const char *const &sep)
{
	C<T, Comp, A> ret(allocator);
	tokens(str, sep, [&ret]
	(const string_view &token)
	{
		ret.emplace(ret.end(), token);
	});

	return ret;
}

template<template<class, class>
         class C,
         class T,
         class A>
C<T, A>
ircd::tokens(const string_view &str,
             const char *const &sep)
{
	A allocator;
	return tokens<C, T, A>(allocator, str, sep);
}

template<template<class, class>
         class C,
         class T,
         class A>
C<T, A>
ircd::tokens(A allocator,
             const string_view &str,
             const char *const &sep)
{
	C<T, A> ret(allocator);
	tokens(str, sep, [&ret]
	(const string_view &token)
	{
		ret.emplace(ret.end(), token);
	});

	return ret;
}

inline size_t
ircd::strlcpy(char *const &dest,
              const string_view &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const auto &len
	{
		src.size() >= max? max - 1 : src.size()
	};

	assert(len < max);
	memcpy(dest, src.data(), len);
	dest[len] = '\0';
	return len;
}

inline size_t
#ifndef HAVE_STRLCPY
ircd::strlcpy(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const auto len{strnlen(src, max)};
	return strlcpy(dest, {src, len}, max);
}
#else
ircd::strlcpy(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	return ::strlcpy(dest, src, max);
}
#endif

inline size_t
#ifndef HAVE_STRLCAT
ircd::strlcat(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	if(!max)
		return 0;

	const ssize_t dsize(strnlen(dest, max));
	const ssize_t ssize(strnlen(src, max));
	const ssize_t ret(dsize + ssize);
	const ssize_t remain(max - dsize);
	const ssize_t cpsz(ssize >= remain? remain - 1 : ssize);
	char *const ptr(dest + dsize);
	memcpy(ptr, src, cpsz);
	ptr[cpsz] = '\0';
	return ret;
}
#else
ircd::strlcat(char *const &dest,
              const char *const &src,
              const size_t &max)
{
	return ::strlcat(dest, src, max);
}
#endif

struct ircd::iless
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	iless(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iless() = default;
};

inline bool
ircd::iless::operator()(const std::string &a,
                        const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::iless::operator()(const string_view &a,
                        const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::iless::operator()(const std::string &a,
                        const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::iless::operator()(const string_view &a,
                        const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) < tolower(b);
	});
}

struct ircd::iequals
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	iequals(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	iequals() = default;
};

inline bool
ircd::iequals::operator()(const std::string &a,
                          const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::iequals::operator()(const string_view &a,
                          const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::iequals::operator()(const std::string &a,
                          const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::iequals::operator()(const string_view &a,
                          const string_view &b)
const
{
	return std::equal(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) == tolower(b);
	});
}

struct ircd::igreater
{
	using is_transparent = std::true_type;

	bool s;

	operator const bool &() const                { return s;                                       }

	bool operator()(const string_view &a, const string_view &b) const;
	bool operator()(const string_view &a, const std::string &b) const;
	bool operator()(const std::string &a, const string_view &b) const;
	bool operator()(const std::string &a, const std::string &b) const;

	template<class A,
	         class B>
	igreater(A&& a, B&& b)
	:s{operator()(std::forward<A>(a), std::forward<B>(b))}
	{}

	igreater() = default;
};

inline bool
ircd::igreater::operator()(const std::string &a,
                           const std::string &b)
const
{
	return operator()(string_view{a}, string_view{b});
}

inline bool
ircd::igreater::operator()(const string_view &a,
                           const std::string &b)
const
{
	return operator()(a, string_view{b});
}

inline bool
ircd::igreater::operator()(const std::string &a,
                           const string_view &b)
const
{
	return operator()(string_view{a}, b);
}

inline bool
ircd::igreater::operator()(const string_view &a,
                           const string_view &b)
const
{
	return std::lexicographical_compare(begin(a), end(a), begin(b), end(b), []
	(const char &a, const char &b)
	{
		return tolower(a) > tolower(b);
	});
}

template<class T>
ircd::string_view
ircd::lex_cast(const T &t)
{
	return lex_cast<T>(t, nullptr, 0);
}

template<>
inline std::string
ircd::lex_cast<std::string>(const string_view &s)
{
	return std::string{s};
}

template<class T>
T
ircd::lex_cast(const string_view &s)
{
	return s;
}

template<>
inline std::string_view
ircd::lex_cast<std::string_view>(const std::string_view &s)
{
	return s;
}

template<>
__attribute__((warning("unnecessary lexical cast")))
inline std::string
ircd::lex_cast<std::string>(const std::string &s)
{
	return s;
}

template<class T>
T
ircd::lex_cast(const std::string &s)
{
	return lex_cast<T>(string_view{s});
}

template<>
inline std::string &
ircd::lex_cast(std::string &s)
{
	return s;
}

template<class T>
T
ircd::lex_cast(std::string &s)
{
	return lex_cast<T>(string_view{s});
}

template<>
inline ircd::string_view
ircd::lex_cast(const string_view &s,
               char *const &buf,
               const size_t &max)
{
	s.copy(buf, max);
	return { buf, max };
}

template<>
inline ircd::string_view
ircd::lex_cast(const std::string_view &s,
               char *const &buf,
               const size_t &max)
{
	s.copy(buf, max);
	return { buf, max };
}

template<>
inline ircd::string_view
ircd::lex_cast(const std::string &s,
               char *const &buf,
               const size_t &max)
{
	s.copy(buf, max);
	return { buf, max };
}

template<class T>
__attribute__((error("unsupported lexical cast")))
ircd::string_view
ircd::lex_cast(T t,
               char *const &buf,
               const size_t &max)
{
	assert(0);
	return {};
}

template<>
inline bool
ircd::try_lex_cast<ircd::string_view>(const string_view &)
{
	return true;
}

template<>
inline bool
ircd::try_lex_cast<std::string_view>(const string_view &)
{
	return true;
}

template<>
inline bool
ircd::try_lex_cast<std::string>(const string_view &s)
{
	return true;
}

template<class T>
__attribute__((error("unsupported lexical cast")))
bool
ircd::try_lex_cast(const string_view &s)
{
	assert(0);
	return false;
}
