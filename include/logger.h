
// ============================================================ //
//
//   d88888D db   d8b   db  .d8b.  db    db
//   YP  d8' 88   I8I   88 d8' `8b `8b  d8'
//      d8'  88   I8I   88 88ooo88  `8bd8'
//     d8'   Y8   I8I   88 88~~~88    88
//    d8' db `8b d8'8b d8' 88   88    88
//   d88888P  `8b8' `8d8'  YP   YP    YP
//
//   open-source, cross-platform, crypto-messenger
//
//   Copyright (C) 2012-2016  Marc Weiler
//
// ============================================================ //

#ifndef SERVER_LOG_H_
#define SERVER_LOG_H_

#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>

BOOST_LOG_GLOBAL_LOGGER(main_logger, boost::log::sources::severity_logger_mt<int>)

// ============================================================ //

#define LOG_INFO BOOST_LOG_SEV(main_logger::get(), boost::log::trivial::severity_level::info)

#define LOG_ERROR BOOST_LOG_SEV(main_logger::get(), boost::log::trivial::severity_level::error)

#define LOG_WARNING BOOST_LOG_SEV(main_logger::get(), boost::log::trivial::severity_level::warning)

// ============================================================ //

#endif
