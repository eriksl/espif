#ifndef _espif_h_
#define _espif_h_

#include "generic_socket.h"
#include "util.h"
#include "command.h"

enum
{
	_flash_sector_size = 4096,
};

class Espif
{
	public:

		Espif(std::string host, std::string command_port,
					bool verbose = false, bool debug = false, bool broadcast = false, bool multicast = false, bool use_tcp = false, bool raw = false,
					bool no_provide_checksum = false, bool no_request_checksum = false, bool dontwait = false,
					unsigned int broadcast_group_mask = 0, unsigned int multicast_burst = 0, unsigned int flash_sector_size = _flash_sector_size);
		Command &command();
		Util &util();

	private:

		GenericSocket	_channel;
		Util			_util;
		Command			_command;
};

#endif
