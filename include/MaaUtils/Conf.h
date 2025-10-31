#pragma once

#ifdef _MSC_VER
#define MAA_DO_PRAGMA(x) __pragma(x)
#elif defined(__GNUC__)
#define MAA_DO_PRAGMA(x) _Pragma(#x)
#else
#define MAA_DO_PRAGMA(x)
#endif

#ifdef _MSC_VER
#define MAA_SUPPRESS_CV_WARNINGS_BEGIN \
    MAA_DO_PRAGMA(warning(push))       \
    MAA_DO_PRAGMA(warning(disable: 5054 4251 4305 4275 4100 4244 4127))
#define MAA_SUPPRESS_CV_WARNINGS_END MAA_DO_PRAGMA(warning(pop))

#define MAA_SUPPRESS_BOOST_WARNINGS_BEGIN \
    MAA_DO_PRAGMA(warning(push))          \
    MAA_DO_PRAGMA(warning(disable: 4702 4244 4297))
#define MAA_SUPPRESS_BOOST_WARNINGS_END MAA_DO_PRAGMA(warning(pop))
#elif defined(__clang__)
#define MAA_SUPPRESS_CV_WARNINGS_BEGIN                                               \
    MAA_DO_PRAGMA(clang diagnostic push)                                             \
    MAA_DO_PRAGMA(clang diagnostic ignored "-Wdeprecated-enum-enum-conversion")      \
    MAA_DO_PRAGMA(clang diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion") \
    MAA_DO_PRAGMA(clang diagnostic ignored "-Wc11-extensions")                       \
    MAA_DO_PRAGMA(clang diagnostic ignored "-Wunused-but-set-variable")
#define MAA_SUPPRESS_CV_WARNINGS_END MAA_DO_PRAGMA(clang diagnostic pop)
#define MAA_SUPPRESS_BOOST_WARNINGS_BEGIN
#define MAA_SUPPRESS_BOOST_WARNINGS_END
#elif defined(__GNUC__)
#define MAA_SUPPRESS_CV_WARNINGS_BEGIN \
    MAA_DO_PRAGMA(GCC diagnostic push) \
    MAA_DO_PRAGMA(GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion")
#define MAA_SUPPRESS_CV_WARNINGS_END MAA_DO_PRAGMA(GCC diagnostic pop)
#define MAA_SUPPRESS_BOOST_WARNINGS_BEGIN
#define MAA_SUPPRESS_BOOST_WARNINGS_END
#else
#define MAA_SUPPRESS_CV_WARNINGS_BEGIN
#define MAA_SUPPRESS_CV_WARNINGS_END
#define MAA_SUPPRESS_BOOST_WARNINGS_BEGIN
#define MAA_SUPPRESS_BOOST_WARNINGS_END
#endif

#ifdef __GNUC__
#define MAA_AUTO_DEDUCED_ZERO_INIT_START \
    MAA_DO_PRAGMA(GCC diagnostic push)   \
    MAA_DO_PRAGMA(GCC diagnostic ignored "-Wmissing-field-initializers")
#define MAA_AUTO_DEDUCED_ZERO_INIT_END MAA_DO_PRAGMA(GCC diagnostic pop)
#elif defined(__clang__)
#define MAA_AUTO_DEDUCED_ZERO_INIT_START \
    MAA_DO_PRAGMA(clang diagnostic push) \
    MAA_DO_PRAGMA(clang diagnostic ignored "-Wmissing-field-initializers")
#define MAA_AUTO_DEDUCED_ZERO_INIT_END MAA_DO_PRAGMA(clang diagnostic pop)
#else
#define MAA_AUTO_DEDUCED_ZERO_INIT_START
#define MAA_AUTO_DEDUCED_ZERO_INIT_END
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef MAA_VERSION
#define MAA_VERSION "DEBUG_VERSION"
#endif

#define MAA_NS MaaNS
#define MAA_NS_BEGIN \
    namespace MAA_NS \
    {
#define MAA_NS_END }

#define MAA_LOG_NS MAA_NS::LogNS
#define MAA_LOG_NS_BEGIN \
    namespace MAA_LOG_NS \
    {
#define MAA_LOG_NS_END }
