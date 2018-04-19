/*
** Author(s):
**  - Cedric GESTES <gestes@aldebaran-robotics.com>
**
** Copyright (C) 2010, 2012 Aldebaran Robotics
*/


#include <vector>
#include <iostream>
#include <sstream>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "transportserver.hpp"
#include "transportsocket.hpp"

#include <qi/session.hpp>
#include "dataperftimer.hpp"

#include <qi/messaging/servicedirectory.hpp>
#include <qi/messaging/gateway.hpp>

#include "transportserver.hpp"
#include <qi/url.hpp>
#include <qi/type/dynamicobjectbuilder.hpp>
#include "../src/session_p.hpp"

static int gLoopCount = 10000;
static const int gThreadCount = 1;
static bool clientDone = false;
#include <iostream>


class ServerEventPrivate {
public:
  ServerEventPrivate()
  {
    _ts = NULL;
    _session = NULL;
    _dp = new qi::perf::DataPerfTimer("Transport synchronous call");
    _msgRecv = 0;
    _numBytes = 0;
  }

  void onTransportServerNewConnection(qi::TransportSocketPtr socket)
  {
    socket->messageReady.connect(boost::bind<void>(&ServerEventPrivate::onMessageReady, this, _1, socket));
    socket->startReading();
  }

  void onMessageReady(const qi::Message &msg, qi::TransportSocketPtr client)
  {
    _msgRecv++;
    size_t s = msg.buffer().size();
    if (s != _numBytes)
    {
      _dp->stop(1);
      if (_msgRecv != gLoopCount)
        std::cout << "Drop " << gLoopCount - _msgRecv << " messages!" << std::endl;

      _numBytes = s;
       _msgRecv = 0;
      _dp->start(gLoopCount, _numBytes);
    }

  }

public:
  std::map<unsigned int, qi::AnyObject> _services;
  qi::TransportServer                *_ts;
  std::vector<std::string>            _endpoints;
  qi::Session                        *_session;

  qi::perf::DataPerfTimer            *_dp;
  int                                 _msgRecv;
  unsigned int                        _numBytes;
};

class ServerEvent
{
public:
  ServerEvent()
    : _p(new ServerEventPrivate())
  {
  }

  ~ServerEvent()
  {
    delete _p;
  }

  void listen(qi::Session *session, const std::vector<std::string> &endpoints)
  {
    _p->_endpoints = endpoints;
    _p->_session = session;

    qi::Url urlo(_p->_endpoints[0]);
    _p->_ts = new qi::TransportServer();

    _p->_ts->newConnection.connect(boost::bind<void>(&ServerEventPrivate::onTransportServerNewConnection, _p, _1));
    _p->_ts->listen(urlo);
  }


  unsigned int registerService(const std::string &name, qi::AnyObject obj)
  {
    qi::Message     msg;
    qi::ServiceInfo si;
    msg.setType(qi::Message::Type_Event);
    msg.setService(qi::Message::Service_ServiceDirectory);
    msg.setObject(qi::Message::GenericObject_Main);
    msg.setFunction(qi::Message::ServiceDirectoryFunction_RegisterService);

    qi::Buffer b;
    qi::ODataStream d(b);
    si.setName(name);
    si.setProcessId(qi::os::getpid());
    si.setMachineId(qi::os::getMachineId());
    si.setEndpoints(_p->_endpoints);
    d << si;
    msg.setBuffer(b);
    _p->_session->_p->_sdSocket->send(msg);
    //_p->_session->_p->_sdClient._socket->waitForId(msg.id());
    //yes but I will fix it later
    qi::os::sleep(1);
    qi::Message ans;
    //TODO dont do that
    //_p->_session->_p->_sdClient._socket->read(&ans);
    qi::IDataStream dout(ans.buffer());
    unsigned int idx = 0;
    dout >> idx;
    _p->_services[idx] = obj;
    return idx;
  }


  void stop() {
  }

private:
  ServerEventPrivate *_p;
};


int main_client(std::string QI_UNUSED(str))
{
  qi::Session  session;
  session.connect("tcp://127.0.0.1:9559");

  qi::AnyObject obj = session.service("serviceTest");

  for (int i = 0; i < 12; ++i)
  {
    char character = 'c';
    unsigned int numBytes = (unsigned int)pow(2.0f, (int)i);
    std::string requeststr = std::string(numBytes, character);

    for (int j = 0; j < gLoopCount; ++j)
    {
      obj->post("New event");
    }
  }
  return 0;
}

void start_client(int count)
{
  boost::thread thd[100];

  assert(count < 100);

  for (int i = 0; i < count; ++i)
  {
    std::stringstream ss;
    ss << "remote" << i;
    std::cout << "starting thread: " << ss.str() << std::endl;
    thd[i] = boost::thread(boost::bind(&main_client, ss.str()));
  }

  for (int i = 0; i < count; ++i)
    thd[i].join();
  clientDone = true;
}


#include <qi/os.hpp>


int main_gateway()
{
  qi::Gateway       gate;

  gate.attachToServiceDirectory("tcp://127.0.0.1:9559");
  gate.listen("tcp://127.0.0.1:12345");
  std::cout << "ready." << std::endl;
  while (true)
    qi::os::sleep(60);

  return 0;
}

std::string reply(const std::string &msg)
{
  return msg;
}

int main_server()
{
  qi::ServiceDirectory sd;
  sd.listen("tcp://127.0.0.1:9559");
  std::cout << "Service Directory ready." << std::endl;

  qi::Session session;
  qi::DynamicObjectBuilder ob;
  ob.advertiseMethod("reply", &reply);
  qi::AnyObject  obj(ob.object());
  ServerEvent srv;
  session.connect("tcp://127.0.0.1:9559");

  std::vector<std::string> endpoints;
  endpoints.push_back("tcp://127.0.0.1:9559");
  srv.listen(&session, endpoints);
  srv.registerService("serviceTest", obj);
  std::cout << "serviceTest ready." << std::endl;

  while (!clientDone)
    qi::os::sleep(60);

  srv.stop();
  session.close();
  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "--client"))
  {
    int threadc = 1;
    if (argc > 2)
      threadc = atoi(argv[2]);
    start_client(threadc);
  }
  else if (argc > 1 && !strcmp(argv[1], "--server"))
  {
    return main_server();
  }
  else
  {
    //start the server
    boost::thread threadServer1(boost::bind(&main_server));
    qi::os::sleep(1);
    start_client(gThreadCount);
  }
  return 0;
}
