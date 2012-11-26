/*
**  Copyright (C) 2012 Aldebaran Robotics
**  See COPYING for the license
*/

#include <qitype/type.hpp>
#include <qitype/metaobject.hpp>
#include <qitype/signature.hpp>
#include "metaobject_p.hpp"
#include <boost/algorithm/string/predicate.hpp>
#include <iomanip>

namespace qi {

  qi::Atomic<int> MetaObjectPrivate::uid = 1;

  MetaObjectPrivate::MetaObjectPrivate(const MetaObjectPrivate &rhs)
  {
    (*this) = rhs;
  }

  MetaObjectPrivate&  MetaObjectPrivate::operator=(const MetaObjectPrivate &rhs)
  {
    {
      boost::recursive_mutex::scoped_lock sl(rhs._methodsMutex);
      _methodsNameToIdx = rhs._methodsNameToIdx;
      _methods          = rhs._methods;
    }
    {
      boost::recursive_mutex::scoped_lock sl(rhs._eventsMutex);
      _eventsNameToIdx = rhs._eventsNameToIdx;
      _events = rhs._events;
    }
    _index = rhs._index;
    return (*this);
  }

  std::vector<qi::MetaMethod> MetaObjectPrivate::findMethod(const std::string &name)
  {
    boost::recursive_mutex::scoped_lock sl(_methodsMutex);
    std::vector<qi::MetaMethod>         ret;
    MetaObject::MethodMap::iterator     it;
    std::string                         cname(name);

    if (cname.find("::", 0, 2) == std::string::npos)
      cname += "::";

    for (it = _methods.begin(); it != _methods.end(); ++it) {
      qi::MetaMethod &mm = it->second;
      if (boost::starts_with(mm.signature(), cname))
        ret.push_back(mm);
    }
    return ret;
  }

  std::vector<MetaObject::CompatibleMethod> MetaObjectPrivate::findCompatibleMethod(const std::string &nameOrSignature)
  {
    boost::recursive_mutex::scoped_lock sl(_methodsMutex);
    std::vector<MetaObject::CompatibleMethod>         ret;
    MetaObject::MethodMap::iterator     it;
    std::string cname(nameOrSignature);

    //no signature specified fallback on findMethod
    if (cname.find("::", 0, 2) == std::string::npos)
    {
      std::vector<MetaMethod> r = findMethod(cname);
      for (unsigned i=0; i<r.size(); ++i)
        ret.push_back(std::make_pair(r[i], 1.0));
      return ret;
    }

    std::vector<std::string> sigsorig = qi::signatureSplit(nameOrSignature);
    if (sigsorig[1].empty())
      return ret;

    Signature sresolved(sigsorig[2]);

    for (it = _methods.begin(); it != _methods.end(); ++it) {
      qi::MetaMethod &mm = it->second;
      std::vector<std::string> sigs = qi::signatureSplit(mm.signature());

      if (sigsorig[1] != sigs[1])
        continue;
      float score = sresolved.isConvertibleTo(Signature(sigs[2]));
      if (score)
        ret.push_back(std::make_pair(mm, score));
    }
    return ret;
  }

  std::vector<MetaSignal> MetaObjectPrivate::findSignal(const std::string &name)
  {
    boost::recursive_mutex::scoped_lock sl(_eventsMutex);
    std::vector<MetaSignal>           ret;
    MetaObject::SignalMap::iterator it;
    std::string cname(name);
    cname += "::";

    for (it = _events.begin(); it != _events.end(); ++it) {
      MetaSignal &mm = it->second;
      if (boost::starts_with(mm.signature(), cname))
        ret.push_back(mm);
    }
    return ret;
  }

  unsigned int MetaObjectPrivate::addMethod(const std::string& sigret, const std::string& signature, int uid) {
    boost::recursive_mutex::scoped_lock sl(_methodsMutex);
    unsigned int id;
    NameToIdx::iterator it = _methodsNameToIdx.find(signature);
    if (it != _methodsNameToIdx.end()) {
      qiLogVerbose("qi.MetaObject") << "Method("<< it->second << ") already defined (and reused): " << sigret << " " << signature;
      return it->second;
    }
    if (uid >= 0)
      id = uid;
    else
      id = ++_index;

    MetaMethod mm(id, sigret, signature);
    _methods[id] = mm;
    _methodsNameToIdx[signature] = id;
    qiLogDebug("qi.MetaObject") << "Adding method("<< id << "): " << sigret << " " << signature;
    return id;
  }

  unsigned int MetaObjectPrivate::addSignal(const std::string &sig, int uid) {
    boost::recursive_mutex::scoped_lock sl(_eventsMutex);
    unsigned int id;
    NameToIdx::iterator it = _eventsNameToIdx.find(sig);
    if (it != _eventsNameToIdx.end()) {
      qiLogVerbose("qi.MetaObject") << "Signal("<< it->second << ") already defined (and reused): " << sig;
      return it->second;
    }
    if (uid >= 0)
      id = uid;
    else
      id = ++_index;
    MetaSignal ms(id, sig);
    _events[id] = ms;
    _eventsNameToIdx[sig] = id;
    qiLogDebug("qi.MetaObject") << "Adding signal("<< id << "): " << sig;
    return id;
  }

  bool MetaObjectPrivate::addMethods(const MetaObject::MethodMap &mms) {
    boost::recursive_mutex::scoped_lock sl(_methodsMutex);
    MetaObject::MethodMap::const_iterator it;
    unsigned int newUid;

    for (it = mms.begin(); it != mms.end(); ++it) {
      newUid = it->second.uid();
      MetaObject::MethodMap::iterator jt = _methods.find(newUid);
      if (jt != _methods.end())
        return false;
      _methods[newUid] = qi::MetaMethod(newUid, it->second.sigreturn(), it->second.signature());
      _methodsNameToIdx[it->second.signature()] = newUid;
    }
    //todo: update uid
    return true;
  }

  bool MetaObjectPrivate::addSignals(const MetaObject::SignalMap &mms) {
    boost::recursive_mutex::scoped_lock sl(_eventsMutex);
    MetaObject::SignalMap::const_iterator it;
    unsigned int newUid;

    for (it = mms.begin(); it != mms.end(); ++it) {
      newUid = it->second.uid();
      MetaObject::SignalMap::iterator jt = _events.find(newUid);
      if (jt != _events.end())
        return false;
      _events[newUid] = qi::MetaSignal(newUid, it->second.signature());
      _eventsNameToIdx[it->second.signature()] = newUid;
    }
    //todo: update uid
    return true;
  }


  void MetaObjectPrivate::refreshCache()
  {
    unsigned int idx = 0;
    {
      boost::recursive_mutex::scoped_lock sl(_methodsMutex);
      _methodsNameToIdx.clear();
      for (MetaObject::MethodMap::iterator i = _methods.begin();
        i != _methods.end(); ++i)
      {
        _methodsNameToIdx[i->second.signature()] = i->second.uid();
        idx = std::max(idx, i->second.uid());
      }
    }
    {
      boost::recursive_mutex::scoped_lock sl(_eventsMutex);
      _eventsNameToIdx.clear();
      for (MetaObject::SignalMap::iterator i = _events.begin();
        i != _events.end(); ++i)
      {
        _eventsNameToIdx[i->second.signature()] = i->second.uid();
        idx = std::max(idx, i->second.uid());
      }
    }
    _index = idx;
  }

  MetaObject::MetaObject()
  {
    _p = new MetaObjectPrivate();
  }

  MetaObject::MetaObject(const MetaObject &other)
  {
    _p = new MetaObjectPrivate();
    *_p = *(other._p);
  }

  MetaObject& MetaObject::operator=(const MetaObject &other)
  {
    *_p = *(other._p);
    return (*this);
  }

  MetaObject::~MetaObject()
  {
    delete _p;
  }

  MetaMethod *MetaObject::method(unsigned int id) {
    boost::recursive_mutex::scoped_lock sl(_p->_methodsMutex);
    MethodMap::iterator i = _p->_methods.find(id);
    if (i == _p->_methods.end())
      return 0;
    return &i->second;
  }

  const MetaMethod *MetaObject::method(unsigned int id) const {
    boost::recursive_mutex::scoped_lock sl(_p->_methodsMutex);
    MethodMap::const_iterator i = _p->_methods.find(id);
    if (i == _p->_methods.end())
      return 0;
    return &i->second;
  }

  MetaSignal *MetaObject::signal(unsigned int id) {
    boost::recursive_mutex::scoped_lock sl(_p->_eventsMutex);
    SignalMap::iterator i = _p->_events.find(id);
    if (i == _p->_events.end())
      return 0;
    return &i->second;
  }

  const MetaSignal *MetaObject::signal(unsigned int id) const {
    boost::recursive_mutex::scoped_lock sl(_p->_eventsMutex);
    SignalMap::const_iterator i = _p->_events.find(id);
    if (i == _p->_events.end())
      return 0;
    return &i->second;
  }

  int MetaObject::methodId(const std::string &name) const
  {
    return _p->methodId(name);
  }

  int MetaObject::signalId(const std::string &name) const
  {
    return _p->signalId(name);
  }

  MetaObject::MethodMap MetaObject::methodMap() const {
    return _p->_methods;
  }

  MetaObject::SignalMap MetaObject::signalMap() const {
    return _p->_events;
  }

  std::vector<qi::MetaMethod> MetaObject::findMethod(const std::string &name) const
  {
    return _p->findMethod(name);
  }

  std::vector<MetaObject::CompatibleMethod> MetaObject::findCompatibleMethod(const std::string &name) const
  {
    return _p->findCompatibleMethod(name);
  }

  std::vector<MetaSignal> MetaObject::findSignal(const std::string &name) const
  {
    return _p->findSignal(name);
  }

  qi::MetaObject MetaObject::merge(const qi::MetaObject &source, const qi::MetaObject &dest) {
    qi::MetaObject result = source;
    if (!result._p->addMethods(dest.methodMap()))
      qiLogError("BoundObject") << "cant merge metaobject (methods)";
    if (!result._p->addSignals(dest.signalMap()))
      qiLogError("BoundObject") << "cant merge metaobject (signals)";
    return result;
  }

  //MetaObjectBuilder
  class MetaObjectBuilderPrivate {
  public:
    qi::MetaObject metaObject;
  };

  MetaObjectBuilder::MetaObjectBuilder()
    : _p(new MetaObjectBuilderPrivate)
  {
  }

  unsigned int MetaObjectBuilder::addMethod(const std::string& sigret, const std::string& signature, int id) {
    return _p->metaObject._p->addMethod(sigret, signature, id);
  }

  unsigned int MetaObjectBuilder::addSignal(const std::string &sig, int id) {
    return _p->metaObject._p->addSignal(sig, id);
  }

  qi::MetaObject MetaObjectBuilder::metaObject() {
    return _p->metaObject;
  }

}

namespace qi {
  namespace details {
    static int calcOffsetMethod(const qi::MetaObject::MethodMap &mmaps) {
      qi::MetaObject::MethodMap::const_iterator it;
      int max = 0;
      for (it = mmaps.begin(); it != mmaps.end(); ++it) {
        int cur = it->second.sigreturn().size();
        if (cur > max)
          max = cur;
      }
      return max;
    }

    void printMetaObject(std::ostream &stream, const qi::MetaObject &mobj) {
      qi::MetaObject::MethodMap methods = mobj.methodMap();
      int offset = calcOffsetMethod(methods);
      qi::MetaObject::MethodMap::const_iterator it2;

      stream << "methods:" << std::endl;
      for (it2 = methods.begin(); it2 != methods.end(); ++it2) {
        stream << "  " << std::right << std::setfill('0') << std::setw(3) << it2->second.uid() << std::setw(0) << " "
               << std::left << std::setfill(' ') << std::setw(offset) << it2->second.sigreturn() << std::setw(0)
               << " " << it2->second.signature() << std::endl;
      }
      stream << "events:" << std::endl;
      qi::MetaObject::SignalMap events = mobj.signalMap();
      qi::MetaObject::SignalMap::const_iterator it3;
      for (it3 = events.begin(); it3 != events.end(); ++it3) {
        stream << "  " << std::right << std::setfill('0') << std::setw(3) << it3->second.uid() << std::setw(0) << " "
               << std::left << std::setfill(' ') << std::setw(offset) << "" << std::setw(0)
               << " " << it3->second.signature() << std::endl;
      }
    }
  }
}

static qi::MetaObjectPrivate* metaObjectPrivate(qi::MetaObject* p) {
  return p->_p;
}


QI_TYPE_STRUCT_EX(qi::MetaObjectPrivate, ptr->refreshCache();, _methods, _events);
QI_TYPE_REGISTER(::qi::MetaObjectPrivate);


QI_TYPE_STRUCT_BOUNCE_REGISTER(::qi::MetaObject, ::qi::MetaObjectPrivate, metaObjectPrivate);
