#include "espif.h"

Espif::Espif(
	std::string host, std::string command_port,
	bool verbose, bool debug, bool broadcast, bool multicast, bool use_tcp, bool raw,
	bool no_provide_checksum, bool no_request_checksum, bool dontwait,
	unsigned int broadcast_group_mask, unsigned int multicast_burst, unsigned int flash_sector_size)
		:
	_channel(host, command_port, flash_sector_size, use_tcp, !!broadcast, !!multicast, verbose),
	_util(_channel, verbose, debug, raw, !no_provide_checksum, !no_request_checksum, broadcast_group_mask),
	_command(_util, _channel, raw, no_provide_checksum, no_request_checksum, dontwait,
			debug, verbose, broadcast_group_mask, multicast_burst, flash_sector_size)
{
}

Command &Espif::command()
{
	return(_command);
}

Util &Espif::util()
{
	return(_util);
}
