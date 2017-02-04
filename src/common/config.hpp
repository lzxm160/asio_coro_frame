#ifndef CONFIG_HPP
#define	CONFIG_HPP
#include <string.h>
#include <cstdarg>
#include <iostream>
#include <map>
#include "include.hpp"
class iconfig:boost::noncopyable
{
	public:
		static boost::shared_ptr<iconfig> get_instance(const std::string& filename);
	private:
		iconfig(const std::string& filename);
		
	public:
		boost::property_tree::ptree m_pt;
		
		unsigned short m_port;
		unsigned short m_threads;
		std::string m_url;

		std::string m_redis_host;
		unsigned short m_redis_port;
		
		std::string m_log_name;
		std::string m_log_level;
};
extern boost::shared_ptr<iconfig> get_config;
#endif

