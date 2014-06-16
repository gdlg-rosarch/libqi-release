/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

// Disable "'this': used in base member initializer list"
#ifdef _MSC_VER
# pragma warning( push )
# pragma warning(disable: 4355)
#endif

#include <vector>
#include <map>

#include <boost/make_shared.hpp>

#include <qi/anyobject.hpp>
#include "transportserver.hpp"
#include "transportsocket.hpp"
#include "servicedirectory.hpp"
#include <qimessaging/session.hpp>
#include <qimessaging/serviceinfo.hpp>
#include <qi/type/objecttypebuilder.hpp>
#include "serverresult.hpp"
#include "session_p.hpp"
#include <qi/os.hpp>
#include <qi/log.hpp>
#include <qimessaging/url.hpp>
#include "servicedirectory_p.hpp"
#include "server.hpp"
#include <boost/algorithm/string.hpp>

qiLogCategory("qimessaging.servicedirectory");

namespace qi
{


  qi::AnyObject createSDP(ServiceDirectoryBoundObject* self) {
    static qi::ObjectTypeBuilder<ServiceDirectoryBoundObject>* ob = 0;
    static boost::mutex* mutex = 0;
    QI_THREADSAFE_NEW(mutex);
    boost::mutex::scoped_lock lock(*mutex);

    if (!ob)
    {
      ob = new qi::ObjectTypeBuilder<ServiceDirectoryBoundObject>();
      unsigned int id = 0;
      id = ob->advertiseMethod("service", &ServiceDirectoryBoundObject::service);
      assert(id == qi::Message::ServiceDirectoryAction_Service);
      id = ob->advertiseMethod("services", &ServiceDirectoryBoundObject::services);
      assert(id == qi::Message::ServiceDirectoryAction_Services);
      id = ob->advertiseMethod("registerService", &ServiceDirectoryBoundObject::registerService);
      assert(id == qi::Message::ServiceDirectoryAction_RegisterService);
      id = ob->advertiseMethod("unregisterService", &ServiceDirectoryBoundObject::unregisterService);
      assert(id == qi::Message::ServiceDirectoryAction_UnregisterService);
      id = ob->advertiseMethod("serviceReady", &ServiceDirectoryBoundObject::serviceReady);
      assert(id == qi::Message::ServiceDirectoryAction_ServiceReady);
      id = ob->advertiseMethod("updateServiceInfo", &ServiceDirectoryBoundObject::updateServiceInfo);
      assert(id == qi::Message::ServiceDirectoryAction_UpdateServiceInfo);
      id = ob->advertiseSignal("serviceAdded", &ServiceDirectoryBoundObject::serviceAdded);
      assert(id == qi::Message::ServiceDirectoryAction_ServiceAdded);
      id = ob->advertiseSignal("serviceRemoved", &ServiceDirectoryBoundObject::serviceRemoved);
      assert(id == qi::Message::ServiceDirectoryAction_ServiceRemoved);
      id = ob->advertiseMethod("machineId", &ServiceDirectoryBoundObject::machineId);
      assert(id == qi::Message::ServiceDirectoryAction_MachineId);
      ob->advertiseMethod("_socketOfService", &ServiceDirectoryBoundObject::_socketOfService);
      // used locally only, we do not export its id
    }
    return ob->object(self, &AnyObject::deleteGenericObjectOnly);
  }

  ServiceDirectoryBoundObject::ServiceDirectoryBoundObject()
    : ServiceBoundObject(1, Message::GenericObject_Main, createSDP(this), qi::MetaCallType_Direct)
    , servicesCount(0)
  {
  }

  ServiceDirectoryBoundObject::~ServiceDirectoryBoundObject()
  {
    if (!connectedServices.empty())
      qiLogWarning() << "Destroying while connected services remain";
  }

  void ServiceDirectoryBoundObject::onSocketDisconnected(TransportSocketPtr socket, std::string error)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    // clean from idxToSocket
    for (std::map<unsigned int, TransportSocketPtr>::iterator it = idxToSocket.begin(), iend = idxToSocket.end(); it != iend;)
    {
      std::map<unsigned int, TransportSocketPtr>::iterator next = it;
      ++next;
      if (it->second == socket)
        idxToSocket.erase(it);
      it = next;
    }
    // if services were connected behind the socket
    std::map<TransportSocketPtr, std::vector<unsigned int> >::iterator it;
    it = socketToIdx.find(socket);
    if (it == socketToIdx.end()) {
      ServiceBoundObject::onSocketDisconnected(socket, error);
      return;
    }
    // Copy the vector, iterators will be invalidated.
    std::vector<unsigned int> ids = it->second;
    // Always start at the beginning since we erase elements on unregisterService
    // and mess up the iterator
    for (std::vector<unsigned int>::iterator it2 = ids.begin();
         it2 != ids.end();
         ++it2)
    {
      qiLogInfo() << "Service #" << *it2 << " disconnected";
      try {
        unregisterService(*it2);
      } catch (std::runtime_error &) {
        qiLogWarning() << "Cannot unregister service #" << *it2;
      }
    }
    socketToIdx.erase(it);
    ServiceBoundObject::onSocketDisconnected(socket, error);
  }

  std::vector<ServiceInfo> ServiceDirectoryBoundObject::services()
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    std::vector<ServiceInfo> result;
    std::map<unsigned int, ServiceInfo>::const_iterator it;

    for (it = connectedServices.begin(); it != connectedServices.end(); ++it)
      result.push_back(it->second);

    return result;
  }

  ServiceInfo ServiceDirectoryBoundObject::service(const std::string &name)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    std::map<unsigned int, ServiceInfo>::const_iterator servicesIt;
    std::map<std::string, unsigned int>::const_iterator it;

    it = nameToIdx.find(name);
    if (it == nameToIdx.end()) {
      std::stringstream ss;
      ss << "Cannot find service '" << name << "' in index";
      throw std::runtime_error(ss.str());
    }

    unsigned int idx = it->second;

    servicesIt = connectedServices.find(idx);
    if (servicesIt == connectedServices.end()) {
      std::stringstream ss;
      ss << "Cannot find ServiceInfo for service '" << name << "'";
      throw std::runtime_error(ss.str());
    }
    return servicesIt->second;
  }

  unsigned int ServiceDirectoryBoundObject::registerService(const ServiceInfo &svcinfo)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    std::map<std::string, unsigned int>::iterator it;
    it = nameToIdx.find(svcinfo.name());
    if (it != nameToIdx.end())
    {
      std::stringstream ss;
      ss << "Service \"" <<
        svcinfo.name() <<
        "\" (#" << it->second << ") is already registered. " <<
        "Rejecting conflicting registration attempt.";
      qiLogWarning()  << ss.str();
      throw std::runtime_error(ss.str());
    }

    unsigned int idx = ++servicesCount;
    nameToIdx[svcinfo.name()] = idx;
    // Do not add serviceDirectory on the map (socket() == null)
    if (idx != qi::Message::Service_ServiceDirectory)
    {
      socketToIdx[currentSocket()].push_back(idx);
    }
    pendingServices[idx] = svcinfo;
    pendingServices[idx].setServiceId(idx);
    idxToSocket[idx] = currentSocket();
    std::stringstream ss;
    ss << "Registered Service \"" << svcinfo.name() << "\" (#" << idx << ")";
    if (! svcinfo.name().empty() && svcinfo.name()[0] == '_') {
      // Hide services whose name starts with an underscore
      qiLogDebug() << ss.str();
    }
    else
    {
      qiLogInfo() << ss.str();
    }

    qi::UrlVector::const_iterator jt;
    for (jt = svcinfo.endpoints().begin(); jt != svcinfo.endpoints().end(); ++jt)
    {
      qiLogDebug() << "Service \"" << svcinfo.name() << "\" is now on " << jt->str();
    }

    return idx;
  }

  void ServiceDirectoryBoundObject::unregisterService(const unsigned int &idx)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    bool pending = false;
    // search the id before accessing it
    // otherwise operator[] create a empty entry
    std::map<unsigned int, ServiceInfo>::iterator it2;
    it2 = connectedServices.find(idx);
    if (it2 == connectedServices.end()) {
      qiLogVerbose() << "Unregister Service: service #" << idx << " not found in the"
                     << " connected list. Looking in the pending list.";
      it2 = pendingServices.find(idx);
      pending = true;
      if (it2 == pendingServices.end())
      {
        std::stringstream ss;
        ss << "Unregister Service: Can't find service #" << idx;
        qiLogVerbose() << ss.str();
        throw std::runtime_error(ss.str());
      }
    }

    std::string serviceName = it2->second.name();

    std::map<std::string, unsigned int>::iterator it;
    it = nameToIdx.find(serviceName);
    if (it == nameToIdx.end())
    {
      std::stringstream ss;
      ss << "Unregister Service: Mapping error, service #" << idx << " (" << serviceName << ") not in nameToIdx";
      qiLogError() << ss.str();
      throw std::runtime_error(ss.str());
    }


    std::stringstream ss;
    ss << "Unregistered Service \""
          << serviceName
          << "\" (#" << idx << ")";

    if (! serviceName.empty() && serviceName[0] == '_') {
      // Hide services whose name starts with underscore
      qiLogDebug() << ss.str();
    }
    else
    {
      qiLogInfo() << ss.str();
    }

    nameToIdx.erase(it);
    if (pending)
      pendingServices.erase(it2);
    else
      connectedServices.erase(it2);

    // Find and remove serviceId into socketToIdx map
    {
      std::map<TransportSocketPtr , std::vector<unsigned int> >::iterator it;
      for (it = socketToIdx.begin(); it != socketToIdx.end(); ++it) {
        std::vector<unsigned int>::iterator jt;
        for (jt = it->second.begin(); jt != it->second.end(); ++jt) {
          if (*jt == idx) {
            it->second.erase(jt);
            //socketToIdx is erased by onSocketDisconnected
            break;
          }
        }
      }
    }
    if (!boost::algorithm::starts_with(serviceName, "_"))
      serviceRemoved(idx, serviceName);
  }

  void ServiceDirectoryBoundObject::updateServiceInfo(const ServiceInfo &svcinfo)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    std::map<unsigned int, ServiceInfo>::iterator itService;

    for (itService = connectedServices.begin();
         itService != connectedServices.end();
         ++itService)
    {
      if (svcinfo.sessionId() == itService->second.sessionId())
      {
        itService->second.setEndpoints(svcinfo.endpoints());
      }
    }

    itService = connectedServices.find(svcinfo.serviceId());
    if (itService != connectedServices.end())
    {
      connectedServices[svcinfo.serviceId()] = svcinfo;
      return;
    }

    // maybe the service registration was pending...
    itService = pendingServices.find(svcinfo.serviceId());
    if (itService != pendingServices.end())
    {
      pendingServices[svcinfo.serviceId()] = svcinfo;
      return;
    }

    std::stringstream ss;
    ss << "updateServiceInfo: Can't find service #" << svcinfo.serviceId();
    qiLogVerbose() << ss.str();
    throw std::runtime_error(ss.str());
  }

  void ServiceDirectoryBoundObject::serviceReady(const unsigned int &idx)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    // search the id before accessing it
    // otherwise operator[] create a empty entry
    std::map<unsigned int, ServiceInfo>::iterator itService;
    itService = pendingServices.find(idx);
    if (itService == pendingServices.end())
    {
      std::stringstream ss;
      ss << "Can't find pending service #" << idx;
      qiLogError() << ss.str();
      throw std::runtime_error(ss.str());
    }

    std::string serviceName = itService->second.name();
    connectedServices[idx] = itService->second;
    pendingServices.erase(itService);

    if (!boost::algorithm::starts_with(serviceName, "_"))
      serviceAdded(idx, serviceName);
  }


  Session_SD::Session_SD(ObjectRegistrar* server)
    : _server(server)
    , _sdbo(boost::make_shared<ServiceDirectoryBoundObject>())
    , _init(false)
  {
  }

  Session_SD::~Session_SD()
  {
  }

  void Session_SD::updateServiceInfo()
  {
    ServiceInfo si;
    si.setName("ServiceDirectory");
    si.setServiceId(qi::Message::Service_ServiceDirectory);
    si.setMachineId(qi::os::getMachineId());
    si.setEndpoints(_server->endpoints());
    ServiceDirectoryBoundObject *sdbo = static_cast<ServiceDirectoryBoundObject*>(_sdbo.get());
    sdbo->updateServiceInfo(si);
  }

  qi::Future<void> Session_SD::listenStandalone(const qi::Url &address)
  {
    if (_init)
      throw std::runtime_error("Already initialised");
    _init = true;
    _server->addObject(1, _sdbo);

    qiLogInfo() << "ServiceDirectory listener created on " << address.str();
    qi::Future<void> f = _server->listen(address);

    ServiceDirectoryBoundObject *sdbo = static_cast<ServiceDirectoryBoundObject*>(_sdbo.get());

    std::map<unsigned int, ServiceInfo>::iterator it =
        sdbo->connectedServices.find(qi::Message::Service_ServiceDirectory);
    if (it != sdbo->connectedServices.end())
    {
      it->second.setEndpoints(_server->endpoints());
      return f;
    }
    ServiceInfo si;
    si.setName("ServiceDirectory");
    si.setServiceId(qi::Message::Service_ServiceDirectory);
    si.setMachineId(qi::os::getMachineId());
    si.setProcessId(qi::os::getpid());
    si.setSessionId("0");
    si.setEndpoints(_server->endpoints());
    unsigned int regid = sdbo->registerService(si);
    sdbo->serviceReady(qi::Message::Service_ServiceDirectory);
    //serviceDirectory must have id '1'
    assert(regid == qi::Message::Service_ServiceDirectory);

    _server->_server.endpointsChanged.connect(boost::bind(&Session_SD::updateServiceInfo, this));

    return f;
  }

  std::string ServiceDirectoryBoundObject::machineId()
  {
    return qi::os::getMachineId();
  }

  qi::TransportSocketPtr ServiceDirectoryBoundObject::_socketOfService(unsigned int id)
  {
    boost::recursive_mutex::scoped_lock lock(mutex);
    std::map<unsigned int, TransportSocketPtr>::iterator it = idxToSocket.find(id);
    if (it == idxToSocket.end())
      return TransportSocketPtr();
    else
      return it->second;
  }

} // !qi

#ifdef _MSC_VER
# pragma warning( pop )
#endif
