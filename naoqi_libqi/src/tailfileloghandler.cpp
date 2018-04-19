/*
 * Copyright (c) 2012 Aldebaran Robotics. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the COPYING file.
 */

#include <qi/log/tailfileloghandler.hpp>

#include <boost/function.hpp>

/*
 * Boost introduced c++11 scoped_enum feature in 1.48 (at most).
 * A fake one was added in case of compilation without c++11.
 * When including boost/filesystem.hpp, if c++11 is activated, scoped_enum
 * are used, but can only work if boost was also compiled with c++11.
 * On the other hand, if c++11 is not activated, fake scoped_enum are used,
 * but can only work if boost was compiled without c++11.
 * This issue was reported in
 * https://svn.boost.org/trac/boost/ticket/6124
 * https://svn.boost.org/trac/boost/ticket/6779
 * https://svn.boost.org/trac/boost/ticket/10038
 * and fixed in boost 1.57
 *
 * libqi-2.5 is now compiled with C++11 activated. Because of this,
 * boost/filesystem.hpp will add C++11 API, even if boost is not compiled
 * with C++11, leading to a linkage error.
 *
 * Therefore we need to specifically ask for non-c++11 API, but only for
 * boost versions lower than 1.57 (as the bug is solved in later versions.
 *
 */

#if BOOST_VERSION < 105700
#  ifndef BOOST_NO_CXX11_SCOPED_ENUMS
#    define BOOST_NO_CXX11_SCOPED_ENUMS
#    define REMOVE_BOOST_NO_CXX11_SCOPED_ENUMS
#  endif
#endif
#include <boost/filesystem.hpp>
#if defined(BOOST_NO_CXX11_SCOPED_ENUMS) && defined(REMOVE_BOOST_NO_CXX11_SCOPED_ENUMS)
#  undef BOOST_NO_CXX11_SCOPED_ENUMS
#  undef REMOVE_BOOST_NO_CXX11_SCOPED_ENUMS
#endif

#include <boost/bind.hpp>

#include <iomanip>
#include "log_p.hpp"
#include <qi/os.hpp>
#include <cstdio>
#include <boost/thread/mutex.hpp>

#define FILESIZEMAX 1024 * 1024

qiLogCategory("qi.log.tailfileloghandler");

namespace qi {
  namespace log {
    class PrivateTailFileLogHandler
    {
    public:
      FILE* _file;
      std::string _fileName;
      int   _writeSize;
      boost::mutex mutex_;
    };

    TailFileLogHandler::TailFileLogHandler(const std::string& filePath)
      : _p(new PrivateTailFileLogHandler)
    {
      _p->_file = NULL;
      _p->_writeSize = 0;
      _p->_fileName = filePath;

      boost::filesystem::path fPath(_p->_fileName);
      // Create the directory!
      try
      {
        if (!boost::filesystem::exists(fPath.make_preferred().parent_path()))
          boost::filesystem::create_directories(fPath.make_preferred().parent_path());
      }
      catch (const boost::filesystem::filesystem_error &e)
      {
        qiLogWarning() << e.what() << std::endl;
      }

      // Open the file.
      FILE* file = qi::os::fopen(fPath.make_preferred().string().c_str(), "w+");

      if (file)
        _p->_file = file;
      else
        qiLogWarning() << "Cannot open "
                       << filePath << std::endl;
    }


    TailFileLogHandler::~TailFileLogHandler()
    {
      if (_p->_file != NULL)
        fclose(_p->_file);
      delete _p;
    }
    void TailFileLogHandler::log(const qi::LogLevel                 verb,
                                 const qi::Clock::time_point        date,
                                 const qi::SystemClock::time_point  systemDate,
                                 const char                        *category,
                                 const char                        *msg,
                                 const char                        *file,
                                 const char                        *fct,
                                 const int                          line)

    {
      boost::mutex::scoped_lock scopedLock(_p->mutex_);

      if (verb > qi::log::logLevel() || _p->_file == NULL)
      {
        return;
      }
      else
      {
        fseek(_p->_file, 0, SEEK_END);

        std::string logline = qi::detail::logline(qi::log::context(), date, systemDate, category, msg, file, fct, line, verb);
        _p->_writeSize += fprintf(_p->_file, "%s", logline.c_str());
        fflush(_p->_file);
      }

      if (_p->_writeSize > FILESIZEMAX)
      {
        fclose(_p->_file);
        boost::filesystem::path filePath(_p->_fileName);
        boost::filesystem::path oldFilePath(_p->_fileName + ".old");

        boost::filesystem::copy_file(filePath,
                                     oldFilePath,
                                     boost::filesystem::copy_option::overwrite_if_exists);

        FILE* pfile = qi::os::fopen(filePath.make_preferred().string().c_str(), "w+");

        _p->_file = pfile;
        _p->_writeSize = 0;
      }
    }
  }
}
