#pragma once

#define BOOST_PROCESS_USE_STD_FS 1

#include <boost/asio.hpp>

// Boost 1.88 made <boost/process.hpp> refer to Boost.Process v2 and moved the v1 headers that
// MaaUtils uses to <boost/process/v1/>. Probe for those headers instead of gating on BOOST_VERSION:
// Boost modules are packaged as separate ports, so <boost/version.hpp> (owned by boost-config) can
// be newer than the installed boost/process headers. MaaDeps v2.12.6 is such a case, it ships
// boost-config 1.89 together with a boost-process overlaid to 1.85, so BOOST_VERSION is 108900
// while boost/process/v1/ does not exist at all.
#if defined(__has_include)
#if __has_include(<boost/process/v1/child.hpp>)
#define MAAUTILS_BOOST_PROCESS_V1_HEADERS 1
#else
#define MAAUTILS_BOOST_PROCESS_V1_HEADERS 0
#endif
#else
#include <boost/version.hpp>
#if BOOST_VERSION >= 108800
#define MAAUTILS_BOOST_PROCESS_V1_HEADERS 1
#else
#define MAAUTILS_BOOST_PROCESS_V1_HEADERS 0
#endif
#endif

#if MAAUTILS_BOOST_PROCESS_V1_HEADERS
// BOOST_PROCESS_VERSION is read the first time a Boost.Process config header is parsed and cannot be
// changed afterwards, so it is set right before including the v1 headers. It keeps the v1 namespace inline
// in boost::process, which keeps names such as boost::process::child and boost::process::ipstream working
// unqualified.
// This is whole-translation-unit state that the scoping below cannot isolate: it only has an effect when
// these are the first Boost.Process headers parsed in the translation unit. The MaaUtils target therefore
// also defines BOOST_PROCESS_VERSION=1 (PUBLIC), so everything linking it is consistent whatever the include
// order is, and this block stays as the fallback for consumers that only add the include directory.
// For the same reason, including any Boost.Process header (v1 or v2), or a header that includes one, before
// this one puts the translation unit into a mixed configuration that no header can repair.
// BOOST_PROCESS_USE_STD_FS above is the same kind of switch.
// push_macro/pop_macro keep the macro from leaking into the rest of the translation unit, where other code
// may rely on Boost.Process v2.
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

#undef MAAUTILS_BOOST_PROCESS_V1_HEADERS
