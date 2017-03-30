/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_MODS_H

namespace ircd {
namespace mapi {

using magic_t = uint16_t;
using version_t = uint16_t;
struct header;

} // namespace mapi
} // namespace ircd

namespace ircd {
namespace mods {

using mapi::magic_t;
using mapi::version_t;

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, filesystem_error)
IRCD_EXCEPTION(error, invalid_export)
IRCD_EXCEPTION(error, expired_symbol)
IRCD_EXCEPTION(error, undefined_symbol)

extern struct log::log log;

struct paths
:std::vector<std::string>
{
	bool added(const std::string &dir) const;

	bool del(const std::string &dir);
	bool add(const std::string &dir, std::nothrow_t);
	bool add(const std::string &dir);

	paths();
}
extern paths;

struct mod;
struct module
:std::shared_ptr<mod>
{
	std::string name() const;
	std::string path() const;

	bool has(const std::string &name) const;

	template<class T = uint8_t> const T *ptr(const std::string &name) const;
	template<class T = uint8_t> T *ptr(const std::string &name);

	template<class T> const T &get(const std::string &name) const;
	template<class T> T &get(const std::string &name);

	module() = default;
	module(const std::string &name);
	~module() noexcept;
};

template<> const uint8_t *module::ptr<const uint8_t>(const std::string &name) const;
template<> uint8_t *module::ptr<uint8_t>(const std::string &name);

class sym_ptr
:std::weak_ptr<mod>
{
	void *ptr;

  public:
	operator bool() const                        { return !expired();                              }
	bool operator!() const                       { return expired();                               }

	template<class T> const T *get() const;
	template<class T> T *get();

	template<class T> const T *operator->() const;
	template<class T> T *operator->();

	template<class T> const T &operator*() const;
	template<class T> T &operator*();

	sym_ptr(const std::string &modname, const std::string &symname);
	~sym_ptr() noexcept;
};

template<class T>
struct sym_ref
:protected sym_ptr
{
	operator const T &() const                   { return sym_ptr::operator*<T>();                 }
	operator T &()                               { return sym_ptr::operator*<T>();                 }

	using sym_ptr::sym_ptr;
};

std::vector<std::string> symbols(const std::string &fullpath, const std::string &section);
std::vector<std::string> symbols(const std::string &fullpath);
std::vector<std::string> sections(const std::string &fullpath);

// Find module names where symbol resides
bool has_symbol(const std::string &name, const std::string &symbol);
std::vector<std::string> find_symbol(const std::string &symbol);

// returns dir/name of first dir containing 'name' (and this will be a loadable module)
// Unlike libltdl, the reason each individual candidate failed is presented in a vector.
std::string search(const std::string &name, std::vector<std::string> &why);
std::string search(const std::string &name);

// Potential modules available to load
std::forward_list<std::string> available();
bool available(const std::string &name);
bool loaded(const std::string &name);

} // namespace mods
} // namespace ircd

namespace ircd {

using mods::module;                              // Bring struct module into main ircd::

} // namespace ircd

template<class T>
T &
ircd::mods::sym_ptr::operator*()
{
	if(unlikely(expired()))
		throw expired_symbol("The reference to a symbol in another module is no longer valid");

	return *get<T>();
}

template<class T>
T *
ircd::mods::sym_ptr::operator->()
{
	return get<T>();
}

template<class T>
T *
ircd::mods::sym_ptr::get()
{
	return reinterpret_cast<T *>(ptr);
}

template<class T>
const T &
ircd::mods::sym_ptr::operator*()
const
{
	if(unlikely(expired()))
		throw expired_symbol("The const reference to a symbol in another module is no longer valid");

	return *get<T>();
}

template<class T>
const T *
ircd::mods::sym_ptr::operator->()
const
{
	return get<T>();
}

template<class T>
const T *
ircd::mods::sym_ptr::get()
const
{
	return reinterpret_cast<const T *>(ptr);
}

template<class T>
T &
ircd::mods::module::get(const std::string &name)
{
	return *ptr<T>(name);
}

template<class T>
const T &
ircd::mods::module::get(const std::string &name)
const
{
	return *ptr<T>(name);
}

template<class T>
T *
ircd::mods::module::ptr(const std::string &name)
{
	return reinterpret_cast<T *>(ptr<uint8_t>(name));
}

template<class T>
const T *
ircd::mods::module::ptr(const std::string &name)
const
{
	return reinterpret_cast<const T *>(ptr<const uint8_t>(name));
}
