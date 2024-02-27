#ifndef _espifconfig_h_
#define _espifconfig_h_

#include <string>

class EspifConfig
{
	public:

		std::string host;
		std::string command_port = "24";
		bool use_tcp = false;
		bool broadcast = false;
		bool multicast = false;
		bool debug = false;
		bool verbose = false;
		bool dontwait = false;
		unsigned int broadcast_group_mask = 0;
		unsigned int multicast_burst = 3;
		bool raw = false;
		bool provide_checksum = true;
		bool request_checksum = true;
		unsigned int sector_size = 4096;
};

#endif
