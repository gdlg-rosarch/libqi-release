#include <map>
#include <string>
#include <iomanip>

#include <qi/session.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include <qi/iocolor.hpp>
#include <qi/jsoncodec.hpp>

#include "qicli.hpp"

int readNumericInput()
{
  std::string c;
  std::cout << "Enter a service id:";
  std::getline(std::cin, c);
  return atoi(c.c_str());;
}

std::string readAlphaInput()
{
  std::string str;
  std::cout << "Enter a service name:";
  std::getline(std::cin, str);
  return str;
}

void showHelp(const po::options_description &desc)
{
  std::cout << std::left << desc << std::endl;
  std::cout << "sub commands:" << std::endl;
  std::cout << "  info     [<ServicePattern>...]" << std::endl;
  std::cout << "  call     <ServicePattern.MethodPattern> [<JsonParameter>...]" << std::endl;
  std::cout << "  post     <ServicePattern.SignalPattern> [<JsonParameter>...]" << std::endl;
  std::cout << "  get      <ServicePattern.PropertyPattern>..." << std::endl;
  std::cout << "  set      <ServicePattern.PropertyPattern>... <JsonParameter>" << std::endl;
  std::cout << "  watch    <ServicePattern.SignalPattern>..." << std::endl;
  std::cout << "  top      [-i interval] [<ServicePattern> ..]" << std::endl;
  std::cout << "  trace    [<ServicePattern> ..]" << std::endl;
  std::cout << "  log-view" << std::endl;
  std::cout << "  log-send <message>" << std::endl;
  std::cout << "  mod      <module>" << std::endl;
}

bool poDefault(const po::command_line_parser &clp, po::variables_map &vm, const po::options_description &desc)
{
  try {
    po::store(const_cast<po::command_line_parser&>(clp).run(), vm);
    po::notify(vm);
  } catch (std::exception &e) {
    std::cout << desc;
    std::cerr << e.what() << std::endl;
    return false;
  }

  if (vm.count("help"))
  {
    std::cout << desc;
    exit(0);
  }
  return true;
}

std::string getTime()
{
  std::ostringstream msg;
  const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
  boost::posix_time::time_facet *f = new boost::posix_time::time_facet("%H:%M:%S");
  msg.imbue(std::locale(msg.getloc(),f));
  msg << now;
  return msg.str();
}

bool isNumber(const std::string &str)
{
  for (unsigned int i = 0; i < str.length(); ++i) {
    if (!::isdigit(str[i]))
      return false;
  }
  return true;
}

void printError(const std::string &errorStr)
{
  if (errorStr.empty())
    return ;
  std::cout << qi::StreamColor_Red << "ERROR: "
            << errorStr << qi::StreamColor_Reset << std::endl;
}

void printSuccess()
{
  std::cout << qi::StreamColor_Green << "OK" << qi::StreamColor_Reset << std::endl;
}

void printServiceMember(const std::string &service, const std::string &member)
{
  std::cout << qi::StreamColor_Bold << service << "." << member
            << ":" << qi::StreamColor_Reset << " ";
}

std::vector<std::string> parseServiceList(
  const std::vector<std::string>& objectNames,
  const std::vector<std::string>& allServices)
{
  std::vector<std::string> services;
  for (unsigned i=0; i<objectNames.size(); ++i)
  {
    std::vector<std::string> split;
    boost::split(split, objectNames[i], boost::algorithm::is_any_of(","));
    for (unsigned i=0; i<split.size(); ++i)
    {
      if (split[i].empty())
        continue;
      else if (split[i] == "*")
      {
        services.insert(services.end(), allServices.begin(), allServices.end());
      }
      else if (split[i][0] == '-')
      {
        std::string pattern = split[i].substr(1);
        for (unsigned i=0; i<services.size(); ++i)
        {
          if (qi::os::fnmatch(pattern, services[i]))
          {
            services[i] = services[services.size() - 1];
            services.pop_back();
            --i; // don't forget to check the new element we juste swaped in
          }
        }
      }
      else
        services.push_back(split[i]);
    }
  }
  return services;
}

qi::AnyValue decodeArgByCast(const std::string &arg)
{
  try {
    return qi::AnyValue(boost::lexical_cast<qi::int64_t>(arg));
  } catch (const boost::bad_lexical_cast &) {
    try {
      return qi::AnyValue(boost::lexical_cast<double>(arg));
    } catch (const boost::bad_lexical_cast &) {
      try {
        return qi::AnyValue(boost::lexical_cast<std::string>(arg));
      } catch (const boost::bad_lexical_cast &) {
        throw std::runtime_error("error while decoding arg");
      }
    }
  }
}

qi::AnyValue decodeArg(const std::string &arg, bool json)
{
  if (json)
    return qi::decodeJSON(arg);
  else
    return decodeArgByCast(arg);
}

qi::GenericFunctionParameters decodeArgs(const std::vector<std::string> &argList, bool json)
{
  qi::GenericFunctionParameters gvArgList;

  for (unsigned int i = 0; i < argList.size(); ++i)
  {
    qi::AnyValue gvArg = decodeArg(argList[i], json);
    gvArgList.push_back(gvArg.clone());
  }
  return gvArgList;
}

std::ostream &operator<<(std::ostream &os, const std::vector<qi::AnyReference> &gvv)
{
  for (unsigned int i = 0; i < gvv.size(); ++i)
  {
    os << qi::encodeJSON(gvv[i]);
    if (i + 1 != gvv.size())
      os << " ";
  }
  return os;
}
