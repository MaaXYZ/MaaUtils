#pragma once

#define BOOST_PROCESS_USE_STD_FS 1

#include <boost/asio.hpp>
#include <boost/version.hpp>

// Since Boost 1.88, boost/process.hpp refers to Boost.Process v2 and the v1 headers moved under
// boost/process/v1/. MaaUtils still uses the v1 API, so include the v1 headers explicitly and
// define BOOST_PROCESS_VERSION as 1 to keep the v1 namespace inline, which keeps names such as
// boost::process::child and boost::process::ipstream working.
// The macro is only read while these headers are parsed for the first time, so it is scoped with
// push_macro/pop_macro to avoid leaking it into the rest of the translation unit.
#if BOOST_VERSION >= 108800
#pragma push_macro("BOOST_PROCESS_VERSION")
#undef BOOST_PROCESS_VERSION
#define BOOST_PROCESS_VERSION 1
#include <boost/process/v1/child.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/search_path.hpp>
#include <boost/process/v1/start_dir.hpp>
#ifdef _WIN32
#include <boost/process/v1/extend.hpp>
#include <boost/process/v1/windows.hpp>
#endif
#pragma pop_macro("BOOST_PROCESS_VERSION")
#else
#include <boost/process.hpp>
#ifdef _WIN32
#include <boost/process/extend.hpp>
#include <boost/process/windows.hpp>
#endif
#endif
