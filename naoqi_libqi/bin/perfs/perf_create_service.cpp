/**
 * Aldebaran Robotics (c) 2010 All Rights Reserved
 *
 */

#include <iostream>
#include <vector>
#include <string>

#include <qi/session.hpp>
#include <qi/anyobject.hpp>

//#include <gtest/gtest.h>

#include <qi/session.hpp>
#include <qi/anyobject.hpp>
#include <qi/type/dynamicobjectbuilder.hpp>
#include <qi/messaging/gateway.hpp>
#include <qi/os.hpp>
#include <qi/application.hpp>
#include <qi/perf/dataperfsuite.hpp>

static std::string reply(const std::string &msg)
{
  return msg;
}

qi::AnyObject genObject() {
  qi::DynamicObjectBuilder ob;
  ob.advertiseMethod("reply1", &reply);
  ob.advertiseMethod("reply2", &reply);
  ob.advertiseMethod("reply3", &reply);
  ob.advertiseMethod("reply4", &reply);
  ob.advertiseMethod("reply5", &reply);
  ob.advertiseMethod("reply6", &reply);
  ob.advertiseMethod("reply7", &reply);
  return ob.object();
}

int main(int argc, char **argv) {
  qi::Session session;
  session.listenStandalone("tcp://0.0.0.0:0");

  std::string fname;
  if (argc > 2)
    fname = argv[2];
  qi::DataPerfSuite out("qimessaging", "perf_create_service", qi::DataPerfSuite::OutputData_MsgPerSecond, fname);
  qi::DataPerf dp;

  dp.start("create_service", 5000);

  for (unsigned int i = 0; i < 5000; ++i) {
    std::stringstream ss;
    ss << "servicetest-"  << i;
    std::cout << "Trying to register " << ss.str() << std::endl;
    // Wait for service id, otherwise register is asynchronous.
    qi::Future<unsigned int> idx = session.registerService(ss.str(), genObject());
    if (idx == 0)
      exit(1);
    std::cout << "registered " << ss.str() << " on " << idx << std::endl;
  }

  dp.stop();
  out << dp;
  out.close();
}
