/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2010 Aldebaran Robotics
*/

#ifndef   	_AL_MESSAGING_TRANSPORT_SERVERBASE_HPP_
# define   	_AL_MESSAGING_TRANSPORT_SERVERBASE_HPP_

# include <althread/altask.h>
# include <alcommon-ng/transport/common/threadable.hpp>
# include <alcommon-ng/transport/common/server_response_delegate.hpp>
# include <alcommon-ng/transport/common/server_command_delegate.hpp>

namespace AL {
  namespace Transport {

    class ServerBase : public Threadable, public AL::ALTask, public internal::ServerResponseDelegate {
    public:
      ServerBase(const std::string &_serverAddress)
        : _serverAddress(_serverAddress),
          _onDataDelegate(0)
      {};
      virtual ~ServerBase() {}

      virtual void            setOnDataDelegate(OnDataDelegate* callback) { _onDataDelegate = callback; }
      virtual OnDataDelegate *getOnDataDelegate() { return _onDataDelegate; }

    protected:
      std::string        _serverAddress;
      OnDataDelegate    *_onDataDelegate;
    };
  }
}

#endif	    /* !_AL_MESSAGING_TRANSPORT_SERVERBASE_HPP_ */
