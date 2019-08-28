// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::fetch
{
	static bool operator<(const request &a, const request &b) noexcept;
	static bool operator<(const request &a, const string_view &b) noexcept;
	static bool operator<(const string_view &a, const request &b) noexcept;

	extern ctx::dock dock;
	extern ctx::mutex requests_mutex;
	extern std::set<request, std::less<>> requests;
	extern std::multimap<room::id, request *> rooms;
	extern ctx::context request_context;
	extern conf::item<size_t> requests_max;
	extern conf::item<seconds> timeout;
	extern conf::item<bool> enable;
	extern log::log log;

	static bool timedout(const request &, const time_t &now);
	static void check_response(const request &, const json::object &);
	static string_view select_origin(request &, const string_view &);
	static string_view select_random_origin(request &);
	static void finish(request &);
	static void retry(request &);
	static bool start(request &, m::v1::event::opts &);
	static bool start(request &);
	static bool handle(request &);

	static void request_handle(const decltype(requests)::iterator &);
	static void request_handle();
	static size_t request_cleanup();
	static void request_worker();

	template<class... args>
	static ctx::future<result>
	submit(const event::id &,
	       const room::id &,
	       const size_t &bufsz = 8_KiB,
	       args&&...);

	static void init();
	static void fini();
}

ircd::mapi::header
IRCD_MODULE
{
    "Event Fetch Unit", ircd::m::fetch::init, ircd::m::fetch::fini
};

decltype(ircd::m::fetch::log)
ircd::m::fetch::log
{
	"m.fetch"
};

decltype(ircd::m::fetch::enable)
ircd::m::fetch::enable
{
	{ "name",     "ircd.m.fetch.enable" },
	{ "default",  true                  },
};

decltype(ircd::m::fetch::timeout)
ircd::m::fetch::timeout
{
	{ "name",     "ircd.m.fetch.timeout" },
	{ "default",  5L                     },
};

decltype(ircd::m::fetch::requests_max)
ircd::m::fetch::requests_max
{
	{ "name",     "ircd.m.fetch.requests.max" },
	{ "default",  256L                        },
};

decltype(ircd::m::fetch::request_context)
ircd::m::fetch::request_context
{
	"m.fetch.req", 512_KiB, &request_worker, context::POST
};

decltype(ircd::m::fetch::rooms)
ircd::m::fetch::rooms;

decltype(ircd::m::fetch::requests)
ircd::m::fetch::requests;

decltype(ircd::m::fetch::requests_mutex)
ircd::m::fetch::requests_mutex;

decltype(ircd::m::fetch::dock)
ircd::m::fetch::dock;

//
// init
//

void
ircd::m::fetch::init()
{
}

void
ircd::m::fetch::fini()
{
	request_context.terminate();
	request_context.join();
	requests.clear();

	assert(requests.empty());
}

//
// <ircd/m/fetch.h>
//

ircd::ctx::future<ircd::m::fetch::result>
IRCD_MODULE_EXPORT
ircd::m::fetch::start(const m::room::id &room_id,
                      const m::event::id &event_id)
{
	ircd::run::changed::dock.wait([]
	{
		return run::level == run::level::RUN ||
		       run::level == run::level::QUIT;
	});

	if(unlikely(run::level != run::level::RUN))
		throw m::UNAVAILABLE
		{
			"Cannot fetch %s in %s in runlevel '%s'",
			string_view{event_id},
			string_view{room_id},
			reflect(run::level)
		};

	dock.wait([]
	{
		return count() < size_t(requests_max);
	});

	return submit(event_id, room_id);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::fetch::count()
{
	return requests.size();
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::exists(const m::event::id &event_id)
{
	return requests.count(string_view{event_id});
}

bool
IRCD_MODULE_EXPORT
ircd::m::fetch::for_each(const std::function<bool (request &)> &closure)
{
	for(auto &request : requests)
		if(!closure(const_cast<fetch::request &>(request)))
			return false;

	return true;
}

//
// internal
//

template<class... args>
ircd::ctx::future<ircd::m::fetch::result>
ircd::m::fetch::submit(const m::event::id &event_id,
                       const m::room::id &room_id,
                       const size_t &bufsz,
                       args&&... a)
{
	assert(room_id && event_id);
	std::unique_lock lock
	{
		requests_mutex
	};

	const scope_notify dock
	{
		fetch::dock
	};

	auto it(requests.lower_bound(string_view(event_id)));
	if(it != end(requests) && it->event_id == event_id)
	{
		assert(it->room_id == room_id);
		return ctx::future<result>{}; //TODO: shared_future.
	}

	it = requests.emplace_hint(it, room_id, event_id, bufsz, std::forward<args>(a)...);
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	ctx::future<result> ret
	{
		request.promise
	};

	start(request);
	return ret;
}

//
// request worker
//

void
ircd::m::fetch::request_worker()
try
{
	while(1)
	{
		dock.wait([]
		{
			return std::any_of(begin(requests), end(requests), []
			(const auto &request)
			{
				return request.started || request.finished;
			});
		});

		request_handle();
	}
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "fetch request worker :%s",
		e.what()
	};

	throw;
}

void
ircd::m::fetch::request_handle()
{
	std::unique_lock lock
	{
		requests_mutex
	};

	if(requests.empty())
		return;

	auto next
	{
		ctx::when_any(requests.begin(), requests.end())
	};

	bool timeout{true};
	{
		const unlock_guard unlock{lock};
		timeout = !next.wait(seconds(timeout), std::nothrow);
	};

	if(timeout)
	{
		request_cleanup();
		return;
	}

	const auto it
	{
		next.get()
	};

	if(it == end(requests))
		return;

	request_handle(it);
	dock.notify_all();
}

void
ircd::m::fetch::request_handle(const decltype(requests)::iterator &it)
{
	auto &request
	{
		const_cast<fetch::request &>(*it)
	};

	if(!request.finished && !handle(request))
		return;

	requests.erase(it);
}

size_t
ircd::m::fetch::request_cleanup()
{
	size_t ret(0);
	const auto now(ircd::time());
	for(auto it(begin(requests)); it != end(requests); ++it)
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		if(!request.started)
		{
			start(request);
			continue;
		}

		if(!request.finished && timedout(request, now))
			retry(request);
	}

	for(auto it(begin(requests)); it != end(requests); )
	{
		auto &request
		{
			const_cast<fetch::request &>(*it)
		};

		if(request.finished)
		{
			it = requests.erase(it);
			++ret;
		}
		else ++it;
	}

	return ret;
}

//
// fetch::request
//

bool
ircd::m::fetch::start(request &request) try
{
	m::v1::event::opts opts;
	opts.dynamic = true;

	assert(request.finished == 0);
	if(!request.started)
		request.started = ircd::time();

	if(!request.origin)
	{
		select_random_origin(request);
		opts.remote = request.origin;
	}

	while(request.origin)
	{
		if(start(request, opts))
			return true;

		select_random_origin(request);
		opts.remote = request.origin;
	}

	assert(!request.finished);
	finish(request);
	return false;
}
catch(...)
{
	assert(!request.finished);
	request.eptr = std::current_exception();
	finish(request);
	return false;
}

bool
ircd::m::fetch::start(request &request,
                      m::v1::event::opts &opts)
try
{
	assert(request.finished == 0);
	if(!request.started)
		request.started = ircd::time();

	request.last = ircd::time(); try
	{
		*static_cast<m::v1::event *>(&request) =
		{
			request.event_id, request.buf, std::move(opts)
		};
	}
	catch(...)
	{
		server::cancel(request);
		throw;
	}

	log::debug
	{
		log, "Starting request for %s in %s from '%s'",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
	};

	dock.notify_all();
	return true;
}
catch(const http::error &e)
{
	log::logf
	{
		log, run::level == run::level::QUIT? log::DERROR: log::ERROR,
		"Starting request for %s in %s to '%s' :%s %s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what(),
		e.content,
	};

	return false;
}
catch(const std::exception &e)
{
	log::logf
	{
		log, run::level == run::level::QUIT? log::DERROR: log::ERROR,
		"Starting request for %s in %s to '%s' :%s",
		string_view{request.event_id},
		string_view{request.room_id},
		string_view{request.origin},
		e.what()
	};

	return false;
}

ircd::string_view
ircd::m::fetch::select_random_origin(request &request)
{
	const m::room::origins origins
	{
		request.room_id
	};

	// copies randomly selected origin into the attempted set.
	const auto closure{[&request]
	(const string_view &origin)
	{
		select_origin(request, origin);
	}};

	// Tests if origin is potentially viable
	const auto proffer{[&request]
	(const string_view &origin)
	{
		// Don't want to request from myself.
		if(my_host(origin))
			return false;

		// Don't want to use a peer we already tried and failed with.
		if(request.attempted.count(origin))
			return false;

		// Don't want to use a peer marked with an error by ircd::server
		if(ircd::server::errmsg(origin))
			return false;

		return true;
	}};

	request.origin = {};
	if(!origins.random(closure, proffer) || !request.origin)
		throw m::NOT_FOUND
		{
			"Cannot find any server to fetch %s in %s",
			string_view{request.event_id},
			string_view{request.room_id},
		};

	return request.origin;
}

ircd::string_view
ircd::m::fetch::select_origin(request &request,
                              const string_view &origin)
{
	const auto iit
	{
		request.attempted.emplace(std::string{origin})
	};

	request.origin = *iit.first;
	return request.origin;
}

bool
ircd::m::fetch::handle(request &request)
{
	request.wait(); try
	{
		const auto code
		{
			request.get()
		};

		const json::object response
		{
			request
		};

		check_response(request, response);

		char pbuf[48];
		log::debug
		{
			log, "Received %u %s good %s in %s from '%s' %s",
			uint(code),
			status(code),
			string_view{request.event_id},
			string_view{request.room_id},
			string_view{request.origin},
			pretty(pbuf, iec(size(string_view(response)))),
		};
	}
	catch(...)
	{
		request.eptr = std::current_exception();

		log::derror
		{
			log, "Erroneous remote for %s in %s from '%s' :%s",
			string_view{request.event_id},
			string_view{request.room_id},
			string_view{request.origin},
			what(request.eptr),
		};
	}

	if(!request.eptr)
		finish(request);
	else
		retry(request);

	return request.finished;
}

void
ircd::m::fetch::retry(request &request)
try
{
	assert(!request.finished);
	assert(request.started && request.last);

	server::cancel(request);
	request.eptr = std::exception_ptr{};
	request.origin = {};
	start(request);
}
catch(...)
{
	request.eptr = std::current_exception();
	finish(request);
}

void
ircd::m::fetch::finish(request &request)
{
	request.finished = ircd::time();

	#if 0
	log::logf
	{
		log, request.eptr? log::DERROR: log::DEBUG,
		"%s in %s started:%ld finished:%d attempted:%zu abandon:%b %S%s",
		string_view{request.event_id},
		string_view{request.room_id},
		request.started,
		request.finished,
		request.attempted.size(),
		!request.promise,
		request.eptr? " :" : "",
		what(request.eptr),
	};
	#endif

	if(!request.promise)
		return;

	if(request.eptr)
	{
		request.promise.set_exception(std::move(request.eptr));
		return;
	}

	request.promise.set_value(result{request});
}

namespace ircd::m::fetch
{
	extern conf::item<bool> check_event_id;
	extern conf::item<bool> check_conforms;
	extern conf::item<int> check_signature;
}

decltype(ircd::m::fetch::check_event_id)
ircd::m::fetch::check_event_id
{
	{ "name",     "ircd.m.fetch.check.event_id" },
	{ "default",  true                          },
};

decltype(ircd::m::fetch::check_conforms)
ircd::m::fetch::check_conforms
{
	{ "name",     "ircd.m.fetch.check.conforms" },
	{ "default",  false                         },
};

decltype(ircd::m::fetch::check_signature)
ircd::m::fetch::check_signature
{
	{ "name",         "ircd.m.fetch.check.signature" },
	{ "default",      true                           },
	{ "description",

	R"(
	false - Signatures of events will not be checked by the fetch unit (they
	are still checked normally during evaluation; this conf item does not
	disable event signature verification for the server).

	true - Signatures of events will be checked by the fetch unit such that
	bogus responses allow the fetcher to try the next server. This check might
	not occur in all cases. It will only occur if the server has the public
	key already; fetch unit worker contexts cannot be blocked trying to obtain
	unknown keys from remote hosts.
	)"},
};

void
ircd::m::fetch::check_response(const request &request,
                               const json::object &response)
{
	const m::event event
	{
		response, request.event_id
	};

	if(check_event_id && !m::check_id(event))
	{
		event::id::buf buf;
		const m::event &claim
		{
			buf, response
		};

		throw ircd::error
		{
			"event::id claim:%s != sought:%s",
			string_view{claim.event_id},
			string_view{request.event_id},
		};
	}

	if(check_conforms)
	{
		thread_local char buf[128];
		const m::event::conforms conforms
		{
			event
		};

		const string_view failures
		{
			conforms.string(buf)
		};

		assert(failures || conforms.clean());
		if(!conforms.clean())
			throw ircd::error
			{
				"Non-conforming event in response :%s",
				failures,
			};
	}

	if(check_signature)
	{
		const string_view &server
		{
			!json::get<"origin"_>(event)?
				user::id(at<"sender"_>(event)).host():
				string_view(json::get<"origin"_>(event))
		};

		const json::object &signatures
		{
			at<"signatures"_>(event).at("server")
		};

		const json::string &key_id
		{
			!signatures.empty()?
				signatures.begin()->first:
				string_view{},
		};

		if(!key_id)
			throw ircd::error
			{
				"Cannot find any keys for '%s' in event.signatures",
				server,
			};

		if(m::keys::cache::has(server, key_id))
			if(!verify(event, server))
				throw ircd::error
				{
					"Signature verification failed."
				};
	}
}

bool
ircd::m::fetch::timedout(const request &request,
                         const time_t &now)
{
	assert(request.started && request.finished >= 0 && request.last != 0);
	return request.last + seconds(timeout).count() < now;
}

bool
ircd::m::fetch::operator<(const request &a,
                          const request &b)
noexcept
{
	return a.event_id < b.event_id;
}

bool
ircd::m::fetch::operator<(const request &a,
                          const string_view &b)
noexcept
{
	return a.event_id < b;
}

bool
ircd::m::fetch::operator<(const string_view &a,
                          const request &b)
noexcept
{
	return a < b.event_id;
}

//
// request::request
//

ircd::m::fetch::request::request(const m::room::id &room_id,
                                 const m::event::id &event_id,
                                 const size_t &bufsz)
:room_id{room_id}
,event_id{event_id}
,buf{bufsz}
{
}

//
// result::result
//

ircd::m::fetch::result::result(request &request)
:m::event
{
	static_cast<json::object>(request)
}
,buf
{
	std::move(request.in.dynamic)
}
{
}
