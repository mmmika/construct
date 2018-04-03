// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/util/params.h>

using namespace ircd;

static void init_cmds();

mapi::header
IRCD_MODULE
{
	"IRCd terminal console: runtime-reloadable self-reflecting command library.", []
	{
		init_cmds();
	}
};

struct cmd
{
	using is_transparent = void;

	static constexpr const size_t &PATH_MAX
	{
		8
	};

	std::string name;
	std::string symbol;
	mods::sym_ptr ptr;

	bool operator()(const cmd &a, const cmd &b) const
	{
		return a.name < b.name;
	}

	bool operator()(const string_view &a, const cmd &b) const
	{
		return a < b.name;
	}

	bool operator()(const cmd &a, const string_view &b) const
	{
		return a.name < b;
	}

	cmd(std::string name, std::string symbol)
	:name{std::move(name)}
	,symbol{std::move(symbol)}
	,ptr{IRCD_MODULE, this->symbol}
	{}

	cmd() = default;
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
};

std::set<cmd, cmd>
cmds;

void
init_cmds()
{
	auto symbols
	{
		mods::symbols(mods::path(IRCD_MODULE))
	};

	for(std::string &symbol : symbols)
	{
		// elide lots of grief by informally finding this first
		if(!has(symbol, "console_cmd"))
			continue;

		thread_local char buf[1024];
		const string_view demangled
		{
			demangle(buf, symbol)
		};

		std::string command
		{
			replace(between(demangled, "__", "("), "__", " ")
		};

		const auto iit
		{
			cmds.emplace(std::move(command), std::move(symbol))
		};

		if(!iit.second)
			throw error
			{
				"Command '%s' already exists", command
			};
	}
}

const cmd *
find_cmd(const string_view &line)
{
	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		const auto it{cmds.lower_bound(name)};
		if(it == end(cmds) || it->name != name)
			continue;

		return &(*it);
	}

	return nullptr;
}

//
// Main command dispatch
//

struct opt
{
	std::ostream &out;
	bool html {false};

	operator std::ostream &()
	{
		return out;
	}

	template<class T> auto &operator<<(T&& t)
	{
		out << std::forward<T>(t);
		return out;
	}

	auto &operator<<(std::ostream &(*manip)(std::ostream &))
	{
		return manip(out);
	}
};

IRCD_EXCEPTION_HIDENAME(ircd::error, bad_command)

int console_command_derived(opt &, const string_view &line);

extern "C" int
console_command(std::ostream &out,
                const string_view &line,
                const string_view &opts)
try
{
	opt opt
	{
		out,
		has(opts, "html")
	};

	const cmd *const cmd
	{
		find_cmd(line)
	};

	if(!cmd)
		return console_command_derived(opt, line);

	const auto args
	{
		lstrip(split(line, cmd->name).second, ' ')
	};

	const auto &ptr{cmd->ptr};
	using prototype = bool (struct opt &, const string_view &);
	return ptr.operator()<prototype>(opt, args);
}
catch(const bad_command &e)
{
	return -2;
}

//
// Help
//

bool
console_cmd__help(opt &out, const string_view &line)
{
	const auto cmd
	{
		find_cmd(line)
	};

	if(cmd)
	{
		out << "No help available for '" << cmd->name << "'."
		    << std::endl;

		//TODO: help string symbol map
	}

	out << "Commands available: \n"
	    << std::endl;

	const size_t elems
	{
		std::min(token_count(line, ' '), cmd::PATH_MAX)
	};

	for(size_t e(elems+1); e > 0; --e)
	{
		const auto name
		{
			tokens_before(line, ' ', e)
		};

		string_view last;
		auto it{cmds.lower_bound(name)};
		if(it == end(cmds))
			continue;

		for(; it != end(cmds); ++it)
		{
			if(!startswith(it->name, name))
				break;

			const auto prefix
			{
				tokens_before(it->name, ' ', e)
			};

			if(last == prefix)
				continue;

			last = prefix;
			const auto suffix
			{
				e > 1? tokens_after(prefix, ' ', e - 2) : prefix
			};

			if(empty(suffix))
				continue;

			out << suffix << std::endl;
		}

		break;
	}

	return true;
}

//
// Test trigger stub
//

bool
console_cmd__test(opt &out, const string_view &line)
{
	return true;
}

//
// Derived commands
//

bool console_id__user(const m::user::id &id, const string_view &args);
bool console_id__room(const m::room::id &id, const string_view &args);
bool console_id__event(const m::event::id &id, const string_view &args);
bool console_json(const json::object &);

int
console_command_derived(opt &out, const string_view &line)
{
	const string_view id{token(line, ' ', 0)};
	const string_view args{tokens_after(line, ' ', 0)};
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::EVENT:
			return console_id__event(id, args);

		case m::id::ROOM:
			return console_id__room(id, args);

		case m::id::USER:
			return console_id__user(id, args);

		default:
			break;
	}

	return -1;
}

//
// Command by JSON
//

bool console_cmd__exec__event(const json::object &);

bool
console_json(const json::object &object)
{
	if(!object.has("type"))
		return true;

	return console_cmd__exec__event(object);
}

//
// Command by ID
//

bool
console_id__event(const m::event::id &id,
                  const string_view &args)
{
	return true;
}

bool
console_id__room(const m::room::id &id,
                 const string_view &args)
{
	return true;
}

bool
console_id__user(const m::user::id &id,
                 const string_view &args)
{
	return true;
}

//
// misc
//


bool
console_cmd__debug(opt &out, const string_view &line)
{
	if(!RB_DEBUG_LEVEL)
	{
		out << "Debugging is not compiled in." << std::endl;
		return true;
	}

	if(log::console_enabled(log::DEBUG))
	{
		out << "Turning off debuglog..." << std::endl;
		log::console_disable(log::DEBUG);
		return true;
	} else {
		out << "Turning on debuglog..." << std::endl;
		log::console_enable(log::DEBUG);
		return true;
	}
}

//
// conf
//

bool
console_cmd__conf__list(opt &out, const string_view &line)
{
	thread_local char val[4_KiB];
	for(const auto &item : conf::items)
		out
		<< std::setw(48) << std::right << item.first
		<< " = " << item.second->get(val)
		<< std::endl;

	return true;
}

//
// mod
//

bool
console_cmd__mod__path(opt &out, const string_view &line)
{
	for(const auto &path : ircd::mods::paths)
		out << path << std::endl;

	return true;
}

bool
console_cmd__mod__list(opt &out, const string_view &line)
{
	auto avflist(mods::available());
	const auto b(std::make_move_iterator(begin(avflist)));
	const auto e(std::make_move_iterator(end(avflist)));
	std::vector<std::string> available(b, e);
	std::sort(begin(available), end(available));

	for(const auto &mod : available)
	{
		const auto loadstr
		{
			mods::loaded(mod)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << loadstr << "] " << mod << std::endl;
	}

	return true;
}

bool
console_cmd__mod__syms(opt &out, const string_view &line)
{
	const std::string path
	{
		token(line, ' ', 0)
	};

	const std::vector<std::string> symbols
	{
		mods::symbols(path)
	};

	for(const auto &sym : symbols)
		out << sym << std::endl;

	out << " -- " << symbols.size() << " symbols in " << path << std::endl;
	return true;
}

bool
console_cmd__mod__reload(opt &out, const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(!m::modules.erase(name))
	{
		out << name << " is not loaded." << std::endl;
		return true;
	}

	m::modules.emplace(name, name);
	out << "reload " << name << std::endl;
	return true;
}

bool
console_cmd__mod__load(opt &out, const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(m::modules.find(name) != end(m::modules))
	{
		out << name << " is already loaded." << std::endl;
		return true;
	}

	m::modules.emplace(name, name);
	return true;
}

bool
console_cmd__mod__unload(opt &out, const string_view &line)
{
	const std::string name
	{
		token(line, ' ', 0)
	};

	if(!m::modules.erase(name))
	{
		out << name << " is not loaded." << std::endl;
		return true;
	}

	out << "unloaded " << name << std::endl;
	return true;
}

//
// db
//

bool
console_cmd__db__prop(opt &out, const string_view &line)
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	const auto property
	{
		token(line, ' ', 1)
	};

	return true;
}

bool
console_cmd__db__txns(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto limit
	{
		lex_cast<size_t>(token(line, ' ', 2, "32"))
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	for_each(database, seqnum, db::seq_closure_bool{[&out, &limit]
	(db::txn &txn, const uint64_t &seqnum) -> bool
	{
		if(txn.has(db::op::SET, "event_id"))
			out << std::setw(12) << std::right << seqnum << " : "
			    << std::get<db::delta::KEY>(txn.get(db::op::SET, "event_id"))
			    << std::endl;

		return --limit;
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__txn(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	if(dbname != "events")
		throw error
		{
			"Sorry, this command is specific to the events db for now."
		};

	const auto seqnum
	{
		lex_cast<uint64_t>(token(line, ' ', 1, "0"))
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	get(database, seqnum, db::seq_closure{[&out]
	(db::txn &txn, const uint64_t &seqnum)
	{
		for_each(txn, [&out, &seqnum]
		(const db::delta &delta)
		{
			out << std::setw(12)  << std::right  << seqnum << " : "
			    << std::setw(8)   << std::left   << reflect(std::get<delta.OP>(delta)) << " "
			    << std::setw(18)  << std::right  << std::get<delta.COL>(delta) << " "
			    << std::get<delta.KEY>(delta)
			    << std::endl;
		});
	}});

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__checkpoint(opt &out, const string_view &line)
try
{
	const auto dbname
	{
		token(line, ' ', 0)
	};

	const auto directory
	{
		token(line, ' ', 1)
	};

	auto &database
	{
		*db::database::dbs.at(dbname)
	};

	checkpoint(database, directory);

	out << "Checkpoint " << name(database)
	    << " to " << directory << " complete."
	    << std::endl;

	return true;
}
catch(const std::out_of_range &e)
{
	out << "No open database by that name" << std::endl;
	return true;
}

bool
console_cmd__db__list(opt &out, const string_view &line)
{
	const auto available
	{
		db::available()
	};

	for(const auto &path : available)
	{
		const auto name
		{
			lstrip(path, db::path("/"))
		};

		const auto it
		{
			db::database::dbs.find(name)
		};

		const auto &light
		{
			it != end(db::database::dbs)? "\033[1;42m \033[0m" : " "
		};

		out << "[" << light << "]"
		    << " " << name
		    << " `" << path << "'"
		    << std::endl;
	}

	return true;
}

//
// net
//

static bool
html__net__peer(opt &out, const string_view &line)
{
	out << "<table>";

	out << "<tr>";
	out << "<td> HOST </td>";
	out << "<td> ADDR </td>";
	out << "<td> LINKS </td>";
	out << "<td> REQS </td>";
	out << "<td> ▲ BYTES Q</td>";
	out << "<td> ▼ BYTES Q</td>";
	out << "<td> ▲ BYTES</td>";
	out << "<td> ▼ BYTES</td>";
	out << "<td> ERROR </td>";
	out << "</tr>";

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << "<tr>";

		out << "<td>" << host << "</td>";
		out << "<td>" << ipp << "</td>";

		out << "<td>" << peer.link_count() << "</td>";
		out << "<td>" << peer.tag_count() << "</td>";
		out << "<td>" << peer.write_size() << "</td>";
		out << "<td>" << peer.read_size() << "</td>";
		out << "<td>" << peer.write_total() << "</td>";
		out << "<td>" << peer.read_total() << "</td>";

		out << "<td>";
		if(peer.err_has() && peer.err_msg())
			out << peer.err_msg();
		else if(peer.err_has())
			out << "<unknown error>"_sv;
		out << "</td>";

		out << "</tr>";
	}

	out << "</table>";
	return true;
}

bool
console_cmd__net__peer(opt &out, const string_view &line)
{
	if(out.html)
		return html__net__peer(out, line);

	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		out << " " << setw(2) << right << peer.link_count()   << " L"
		    << " " << setw(2) << right << peer.tag_count()    << " T"
		    << " " << setw(9) << right << peer.write_size()   << " UP Q"
		    << " " << setw(9) << right << peer.read_size()    << " DN Q"
		    << " " << setw(9) << right << peer.write_total()  << " UP"
		    << " " << setw(9) << right << peer.read_total()   << " DN"
		    ;

		if(peer.err_has() && peer.err_msg())
			out << "  :" << peer.err_msg();
		else if(peer.err_has())
			out << "  <unknown error>"_sv;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__net__peer__clear(opt &out, const string_view &line)
{
	const net::hostport hp
	{
		token(line, ' ', 0)
	};

	const auto cleared
	{
		server::errclear(hp)
	};

	out << std::boolalpha << cleared << std::endl;
	return true;
}

bool
console_cmd__net__peer__version(opt &out, const string_view &line)
{
	for(const auto &p : server::peers)
	{
		using std::setw;
		using std::left;
		using std::right;

		const auto &host{p.first};
		const auto &peer{*p.second};
		const net::ipport &ipp{peer.remote};

		out << setw(40) << right << host;

		if(ipp)
		    out << ' ' << setw(22) << left << ipp;
		else
		    out << ' ' << setw(22) << left << " ";

		if(!empty(peer.server_name))
			out << " :" << peer.server_name;

		out << std::endl;
	}

	return true;
}

bool
console_cmd__net__host(opt &out, const string_view &line)
{
	const params token
	{
		line, " ", {"host", "service"}
	};

	const auto &host{token.at(0)};
	const auto &service
	{
		token.count() > 1? token.at(1) : string_view{}
	};

	const net::hostport hostport
	{
		host, service
	};

	ctx::dock dock;
	bool done{false};
	net::ipport ipport;
	std::exception_ptr eptr;
	net::dns(hostport, [&done, &dock, &eptr, &ipport]
	(std::exception_ptr eptr_, const net::ipport &ipport_)
	{
		eptr = std::move(eptr_);
		ipport = ipport_;
		done = true;
		dock.notify_one();
	});

	while(!done)
		dock.wait();

	if(eptr)
		std::rethrow_exception(eptr);
	else
		out << ipport << std::endl;

	return true;
}

bool
console_cmd__net__host__cache(opt &out, const string_view &line)
{
	switch(hash(token(line, " ", 0)))
	{
		case hash("A"):
		{
			for(const auto &pair : net::dns::cache.A)
			{
				const auto &host{pair.first};
				const auto &record{pair.second};
				const net::ipport ipp{record.ip4, 0};
				out << std::setw(32) << host
				    << " => " << ipp
				    <<  " expires " << timestr(record.ttl, ircd::localtime)
				    << " (" << record.ttl << ")"
				    << std::endl;
			}

			return true;
		}

		case hash("SRV"):
		{
			for(const auto &pair : net::dns::cache.SRV)
			{
				const auto &key{pair.first};
				const auto &record{pair.second};
				const net::hostport hostport
				{
					record.tgt, record.port
				};

				out << std::setw(32) << key
				    << " => " << hostport
				    <<  " expires " << timestr(record.ttl, ircd::localtime)
				    << " (" << record.ttl << ")"
				    << std::endl;
			}

			return true;
		}

		default: throw bad_command
		{
			"Which cache?"
		};
	}
}

//
// key
//

bool
console_cmd__key(opt &out, const string_view &line)
{
	out << "origin:                  " << m::my_host() << std::endl;
	out << "public key ID:           " << m::self::public_key_id << std::endl;
	out << "public key base64:       " << m::self::public_key_b64 << std::endl;
	out << "TLS cert sha256 base64:  " << m::self::tls_cert_der_sha256_b64 << std::endl;

	return true;
}

bool
console_cmd__key__get(opt &out, const string_view &line)
{
	const auto server_name
	{
		token(line, ' ', 0)
	};

	m::keys::get(server_name, [&out]
	(const auto &keys)
	{
		out << keys << std::endl;
	});

	return true;
}

bool
console_cmd__key__fetch(opt &out, const string_view &line)
{

	return true;
}

//
// event
//

bool
console_cmd__event(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	static char buf[64_KiB];
	const m::event event
	{
		event_id, buf
	};

	if(!empty(args)) switch(hash(token(args, ' ', 0)))
	{
		case hash("raw"):
			out << json::object{buf} << std::endl;
			return true;
	}

	out << pretty(event) << std::endl;
	return true;
}

bool
console_cmd__event__erase(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	m::dbs::write(txn, event, opts);
	txn();

	out << "erased " << txn.size() << " cells"
	    << " for " << event_id << std::endl;

	return true;
}

bool
console_cmd__event__dump(opt &out, const string_view &line)
{
	const auto filename
	{
		token(line, ' ', 0)
	};

	db::column column
	{
		*m::dbs::events, "event_id"
	};

	static char buf[512_KiB];
	char *pos{buf};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	m::event::fetch event;
	for(auto it(begin(column)); it != end(column); ++it, ++ecount)
	{
		const auto remain
		{
			size_t(buf + sizeof(buf) - pos)
		};

		assert(remain >= 64_KiB && remain <= sizeof(buf));
		const mutable_buffer mb{pos, remain};
		const string_view event_id{it->second};
		seek(event, event_id, std::nothrow);
		if(unlikely(!event.valid(event_id)))
		{
			++errcount;
			continue;
		}

		pos += json::print(mb, event);
		if(pos + 64_KiB > buf + sizeof(buf))
		{
			const const_buffer cb{buf, pos};
			foff += size(fs::append(filename, cb));
			pos = buf;
			++acount;
		}
	}

	if(pos > buf)
	{
		const const_buffer cb{buf, pos};
		foff += size(fs::append(filename, cb));
		++acount;
	}

	out << "Dumped " << ecount << " events"
	    << " using " << foff << " bytes"
	    << " in " << acount << " writes"
	    << " to " << filename
	    << " with " << errcount << " errors"
	    << std::endl;

	return true;
}

bool
console_cmd__event__fetch(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	const auto host
	{
		!empty(args)? token(args, ' ', 0) : ""_sv
	};

	m::v1::event::opts opts;
	if(host)
		opts.remote = host;

	static char buf[96_KiB];
	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const m::event event
	{
		request
	};

	out << json::object{request} << std::endl;
	out << std::endl;
	out << pretty(event) << std::endl;
	return true;
}

//
// state
//

bool
console_cmd__state__count(opt &out, const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	out << m::state::count(root) << std::endl;
	return true;
}

bool
console_cmd__state__each(opt &out, const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view root
	{
		arg
	};

	m::state::for_each(root, type, [&out]
	(const string_view &key, const string_view &val)
	{
		out << key << " => " << val << std::endl;
	});

	return true;
}

bool
console_cmd__state__get(opt &out, const string_view &line)
{
	const string_view root
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	m::state::get(root, type, state_key, [&out]
	(const auto &value)
	{
		out << "got: " << value << std::endl;
	});

	return true;
}

bool
console_cmd__state__dfs(opt &out, const string_view &line)
{
	const string_view arg
	{
		token(line, ' ', 0)
	};

	const string_view root
	{
		arg
	};

	m::state::dfs(root, [&out]
	(const auto &key, const string_view &val, const uint &depth, const uint &pos)
	{
		out << std::setw(2) << depth << " + " << pos
		    << " : " << key << " => " << val
		    << std::endl;

		return true;
	});

	return true;
}

bool
console_cmd__state__root(opt &out, const string_view &line)
{
	const m::event::id event_id
	{
		token(line, ' ', 0)
	};

	char buf[m::state::ID_MAX_SZ];
	out << m::dbs::state_root(buf, event_id) << std::endl;
	return true;
}

//
// commit
//

bool
console_cmd__commit(opt &out, const string_view &line)
{
	m::event event
	{
		json::object{line}
	};

	return true;
}

//
// exec
//

bool
console_cmd__exec__event(const json::object &event)
{
	m::vm::opts opts;
	opts.verify = false;
	m::vm::eval eval
	{
		opts
	};

	eval(event);
	return true;
}

bool
console_cmd__exec__file(opt &out, const string_view &line)
{
	const params token{line, " ",
	{
		"file path", "limit", "start", "room_id/event_id/sender"
	}};

	const auto path
	{
		token.at(0)
	};

	const auto limit
	{
		token.at<size_t>(1)
	};

	const auto start
	{
		token[2]? lex_cast<size_t>(token[2]) : 0
	};

	const string_view id{token[3]};
	const string_view room_id
	{
		id && m::sigil(id) == m::id::ROOM? id : string_view{}
	};

	const string_view event_id
	{
		id && m::sigil(id) == m::id::EVENT? id : string_view{}
	};

	const string_view sender
	{
		id && m::sigil(id) == m::id::USER? id : string_view{}
	};

	m::vm::opts opts;
	opts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	opts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	opts.prev_check_exists = false;
	opts.notify = false;
	opts.verify = false;
	m::vm::eval eval
	{
		opts
	};

	size_t foff{0};
	size_t i(0), j(0), r(0);
	for(; !limit || i < limit; ++r)
	{
		static char buf[512_KiB];
		const string_view read
		{
			ircd::fs::read(path, buf, foff)
		};

		size_t boff(0);
		json::object object;
		json::vector vector{read};
		for(; boff < size(read) && i < limit; ) try
		{
			object = *begin(vector);
			boff += size(string_view{object});
			vector = { data(read) + boff, size(read) - boff };
			const m::event event
			{
				object
			};

			if(room_id && json::get<"room_id"_>(event) != room_id)
				continue;

			if(event_id && json::get<"event_id"_>(event) != event_id)
				continue;

			if(sender && json::get<"sender"_>(event) != sender)
				continue;

			if(j++ < start)
				continue;

			eval(event);
			++i;
		}
		catch(const json::parse_error &e)
		{
			break;
		}
		catch(const std::exception &e)
		{
			out << fmt::snstringf
			{
				128, "Error at i=%zu j=%zu r=%zu foff=%zu boff=%zu\n",
				i, j, r, foff, boff
			};

			out << string_view{object} << std::endl;
			out << e.what() << std::endl;
			return true;
		}

		foff += boff;
	}

	out << "Executed " << i
	    << " of " << j << " events"
	    << " in " << foff << " bytes"
	    << " using " << r << " reads"
	    << std::endl;

	return true;
}

//
// room
//

bool
console_cmd__room__head(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << head(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__depth(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	out << depth(room_id) << std::endl;
	return true;
}

bool
console_cmd__room__members(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view membership
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : string_view{}
	};

	const m::room room
	{
		room_id
	};

	const m::room::members members
	{
		room
	};

	const auto closure{[&out](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	}};

	if(membership)
		members.for_each(membership, closure);
	else
		members.for_each(closure);

	return true;
}

bool
console_cmd__room__origins(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::room room
	{
		room_id
	};

	const m::room::origins origins
	{
		room
	};

	origins.test([&out](const string_view &origin)
	{
		out << origin << std::endl;
		return false;
	});

	return true;
}

bool
console_cmd__room__state(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const auto &event_id
	{
		token(line, ' ', 1, {})
	};

	const m::room room
	{
		room_id, event_id
	};

	const m::room::state state
	{
		room
	};

	state.for_each([&out](const m::event &event)
	{
		out << pretty_oneline(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__count(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view &type
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : string_view{}
	};

	const m::room room
	{
		room_id
	};

	const m::room::state state
	{
		room
	};

	if(type)
		out << state.count(type) << std::endl;
	else
		out << state.count() << std::endl;

	return true;
}

bool
console_cmd__room__messages(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const int64_t depth
	{
		token_count(line, ' ') > 1? lex_cast<int64_t>(token(line, ' ', 1)) : -1
	};

	const char order
	{
		token_count(line, ' ') > 2? token(line, ' ', 2).at(0) : 'b'
	};

	const m::room room
	{
		room_id
	};

	m::room::messages it{room};
	if(depth >= 0)
		it.seek(depth);

	for(; it; order == 'b'? --it : ++it)
		out << pretty_oneline(*it) << std::endl;

	return true;
}

bool
console_cmd__room__get(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const string_view type
	{
		token(line, ' ', 1)
	};

	const string_view state_key
	{
		token(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	room.get(type, state_key, [&out](const m::event &event)
	{
		out << pretty(event) << std::endl;
	});

	return true;
}

bool
console_cmd__room__set(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view type
	{
		token(line, ' ', 2)
	};

	const string_view state_key
	{
		token(line, ' ', 3)
	};

	const json::object &content
	{
		token(line, ' ', 4)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		send(room, sender, type, state_key, content)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__message(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::user::id &sender
	{
		token(line, ' ', 1)
	};

	const string_view body
	{
		tokens_after(line, ' ', 1)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		message(room, sender, body)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__redact(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	const m::event::id &redacts
	{
		token(line, ' ', 1)
	};

	const m::user::id &sender
	{
		token(line, ' ', 2)
	};

	const string_view reason
	{
		tokens_after(line, ' ', 2)
	};

	const m::room room
	{
		room_id
	};

	const auto event_id
	{
		redact(room, sender, redacts, reason)
	};

	out << event_id << std::endl;
	return true;
}

bool
console_cmd__room__join(opt &out, const string_view &line)
{
	const string_view room_id_or_alias
	{
		token(line, ' ', 0)
	};

	const m::user::id &user_id
	{
		token(line, ' ', 1)
	};

	const string_view &event_id
	{
		token(line, ' ', 2, {})
	};

	switch(m::sigil(room_id_or_alias))
	{
		case m::id::ROOM:
		{
			const m::room room
			{
				room_id_or_alias, event_id
			};

			const auto join_event
			{
				m::join(room, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		case m::id::ROOM_ALIAS:
		{
			const m::room::alias alias
			{
				room_id_or_alias
			};

			const auto join_event
			{
				m::join(alias, user_id)
			};

			out << join_event << std::endl;
			return true;
		}

		default: throw error
		{
			"Don't know how to join '%s'", room_id_or_alias
		};
	}

	return true;
}

bool
console_cmd__room__id(opt &out, const string_view &id)
{
	if(m::has_sigil(id)) switch(m::sigil(id))
	{
		case m::id::USER:
			out << m::user{id}.room_id() << std::endl;
			break;

		case m::id::NODE:
			out << m::node{id}.room_id() << std::endl;
			break;

		case m::id::ROOM_ALIAS:
			out << m::room_id(m::room::alias(id)) << std::endl;
			break;

		default:
			break;
	}

	return true;
}

bool
console_cmd__room__purge(opt &out, const string_view &line)
{
	const m::room::id room_id
	{
		token(line, ' ', 0)
	};

	return true;
}

//
// fed
//

bool
console_cmd__fed__groups(opt &out, const string_view &line)
{
	const m::id::node &node
	{
		token(line, ' ', 0)
	};

	const auto args
	{
		tokens_after(line, ' ', 0)
	};

	m::user::id ids[8];
	string_view tok[8];
	const auto count{std::min(tokens(args, ' ', tok), size_t(8))};
	std::copy(begin(tok), begin(tok) + count, begin(ids));

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::groups::publicised::opts opts;
	m::v1::groups::publicised request
	{
		node, vector_view<const m::user::id>(ids, count), buf, std::move(opts)
	};

	if(request.wait(seconds(10)) == ctx::future_status::timeout)
		throw http::error{http::REQUEST_TIMEOUT};

	request.get();
	const json::object response
	{
		request.in.content
	};

	std::cout << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__head(opt &out, const string_view &line)
{
	const m::room::id &room_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1)
	};

	thread_local char buf[16_KiB];
	m::v1::make_join request
	{
		room_id, m::me.user_id, buf
	};

	if(request.wait(seconds(5)) == ctx::future_status::timeout)
		throw http::error{http::REQUEST_TIMEOUT};

	request.get();
	const json::object proto
	{
		request.in.content
	};

	const json::array prev_events
	{
		proto.at({"event", "prev_events"})
	};

	for(const json::array &prev_event : prev_events)
	{
		const string_view &id{prev_event.at(0)};
		out << id << " :" << string_view{prev_event.at(1)} << std::endl;
	}

	return true;
}

bool
console_cmd__fed__state(opt &out, const string_view &line)
{
	const m::room::id &room_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, room_id.host())
	};

	string_view event_id
	{
		token(line, ' ', 2, {})
	};

	string_view op
	{
		token(line, ' ', 3, {})
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(seconds(30));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const json::array &auth_chain
	{
		response["auth_chain"]
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		for(const json::object &event : auth_chain)
			out << pretty_oneline(m::event{event}) << std::endl;

		for(const json::object &event : pdus)
			out << pretty_oneline(m::event{event}) << std::endl;

		return true;
	}

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.notify = false;
	m::vm::eval eval
	{
		vmopts
	};

	for(const json::object &event : auth_chain)
		eval(event);

	for(const json::object &event : pdus)
		eval(event);

	return true;
}

bool
console_cmd__fed__state_ids(opt &out, const string_view &line)
{
	const m::room::id &room_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, room_id.host())
	};

	string_view event_id
	{
		token(line, ' ', 2, {})
	};

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[8_KiB];
	m::v1::state::opts opts;
	opts.remote = remote;
	opts.event_id = event_id;
	opts.ids_only = true;
	m::v1::state request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(seconds(30));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const json::array &auth_chain
	{
		response["auth_chain_ids"]
	};

	const json::array &pdus
	{
		response["pdu_ids"]
	};

	for(const string_view &event_id : auth_chain)
		out << unquote(event_id) << std::endl;

	for(const string_view &event_id : pdus)
		out << unquote(event_id) << std::endl;

	return true;
}

bool
console_cmd__fed__backfill(opt &out, const string_view &line)
{
	const m::room::id &room_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1)
	};

	const string_view &count
	{
		token(line, ' ', 2, "32")
	};

	string_view event_id
	{
		token(line, ' ', 3, {})
	};

	string_view op
	{
		token(line, ' ', 4, {})
	};

	if(!op && event_id == "eval")
		std::swap(op, event_id);

	// Used for out.head, out.content, in.head, but in.content is dynamic
	thread_local char buf[16_KiB];
	m::v1::backfill::opts opts;
	opts.remote = remote;
	opts.limit = lex_cast<size_t>(count);
	if(event_id)
		opts.event_id = event_id;

	m::v1::backfill request
	{
		room_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const json::array &pdus
	{
		response["pdus"]
	};

	if(op != "eval")
	{
		for(const json::object &event : pdus)
			out << pretty_oneline(m::event{event}) << std::endl;

		return true;
	}

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.non_conform.set(m::event::conforms::MISSING_MEMBERSHIP);
	vmopts.prev_check_exists = false;
	vmopts.notify = false;
	m::vm::eval eval
	{
		vmopts
	};

	for(const json::object &event : pdus)
		eval(event);

	return true;
}

bool
console_cmd__fed__event(opt &out, const string_view &line)
{
	const m::event::id &event_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, event_id.host())
	};

	m::v1::event::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::event request
	{
		event_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	const m::event event
	{
		response
	};

	out << pretty(event) << std::endl;

	if(!verify(event))
		out << "- SIGNATURE FAILED" << std::endl;

	if(!verify_hash(event))
		out << "- HASH MISMATCH: " << b64encode_unpadded(hash(event)) << std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out << "- " << conforms << std::endl;

	return true;
}

bool
console_cmd__fed__query__profile(opt &out, const string_view &line)
{
	const m::user::id &user_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : user_id.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::profile request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__query__directory(opt &out, const string_view &line)
{
	const m::id::room_alias &room_alias
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token_count(line, ' ') > 1? token(line, ' ', 1) : room_alias.host()
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::query::directory request
	{
		room_alias, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__query__user_devices(opt &out, const string_view &line)
{
	const m::id::user &user_id
	{
		token(line, ' ', 0)
	};

	const net::hostport remote
	{
		token(line, ' ', 1, user_id.host())
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::query::user_devices request
	{
		user_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__query__client_keys(opt &out, const string_view &line)
{
	const m::id::user &user_id
	{
		token(line, ' ', 0)
	};

	const string_view &device_id
	{
		token(line, ' ', 1)
	};

	const net::hostport remote
	{
		token(line, ' ', 2, user_id.host())
	};

	m::v1::query::opts opts;
	opts.remote = remote;

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	m::v1::query::client_keys request
	{
		user_id, device_id, buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}

bool
console_cmd__fed__version(opt &out, const string_view &line)
{
	const net::hostport remote
	{
		token(line, ' ', 0)
	};

	m::v1::version::opts opts;
	opts.remote = remote;

	thread_local char buf[8_KiB];
	m::v1::version request
	{
		buf, std::move(opts)
	};

	request.wait(seconds(10));
	const auto code
	{
		request.get()
	};

	const json::object &response
	{
		request
	};

	out << string_view{response} << std::endl;
	return true;
}
