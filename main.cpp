#include "espif.h"
#include "exception.h"

#include <iostream>
#include <boost/program_options.hpp>

enum
{
	flash_sector_size = 4096,
};

namespace po = boost::program_options;

static bool option_raw = false;
static bool option_verbose = false;
static bool option_debug = false;
static bool option_no_provide_checksum = false;
static bool option_no_request_checksum = false;
static bool option_use_tcp = false;
static bool option_dontwait = false;
static unsigned int option_broadcast_group_mask = 0;
static unsigned int option_multicast_burst = 1;

int main(int argc, const char **argv)
{
	po::options_description	options("usage");

	try
	{
		std::vector<std::string> host_args;
		std::string host;
		std::string args;
		std::string command_port;
		std::string filename;
		std::string start_string;
		std::string length_string;
		int start;
		int image_slot;
		int image_timeout;
		int dim_x, dim_y, depth;
		unsigned int length;
		bool nocommit = false;
		bool noreset = false;
		bool notemp = false;
		bool otawrite = false;
		bool cmd_write = false;
		bool cmd_simulate = false;
		bool cmd_verify = false;
		bool cmd_benchmark = false;
		bool cmd_image = false;
		bool cmd_image_epaper = false;
		bool cmd_broadcast = false;
		bool cmd_multicast = false;
		bool cmd_read = false;
		bool cmd_info = false;
		unsigned int selected;

		options.add_options()
			("info,i",					po::bool_switch(&cmd_info)->implicit_value(true),							"INFO")
			("read,R",					po::bool_switch(&cmd_read)->implicit_value(true),							"READ")
			("verify,V",				po::bool_switch(&cmd_verify)->implicit_value(true),							"VERIFY")
			("simulate,S",				po::bool_switch(&cmd_simulate)->implicit_value(true),						"WRITE simulate")
			("write,W",					po::bool_switch(&cmd_write)->implicit_value(true),							"WRITE")
			("benchmark,B",				po::bool_switch(&cmd_benchmark)->implicit_value(true),						"BENCHMARK")
			("image,I",					po::bool_switch(&cmd_image)->implicit_value(true),							"SEND IMAGE")
			("epaper-image,e",			po::bool_switch(&cmd_image_epaper)->implicit_value(true),					"SEND EPAPER IMAGE (uc8151d connected to host)")
			("broadcast,b",				po::bool_switch(&cmd_broadcast)->implicit_value(true),						"BROADCAST SENDER send broadcast message")
			("multicast,M",				po::bool_switch(&cmd_multicast)->implicit_value(true),						"MULTICAST SENDER send multicast message")
			("host,h",					po::value<std::vector<std::string> >(&host_args)->required(),				"host or broadcast address or multicast group to use")
			("verbose,v",				po::bool_switch(&option_verbose)->implicit_value(true),						"verbose output")
			("debug,D",					po::bool_switch(&option_debug)->implicit_value(true),						"packet trace etc.")
			("tcp,t",					po::bool_switch(&option_use_tcp)->implicit_value(true),						"use TCP instead of UDP")
			("filename,f",				po::value<std::string>(&filename),											"file name")
			("start,s",					po::value<std::string>(&start_string)->default_value("-1"),					"send/receive start address (OTA is default)")
			("length,l",				po::value<std::string>(&length_string)->default_value("0x1000"),			"read length")
			("command-port,p",			po::value<std::string>(&command_port)->default_value("24"),					"command port to connect to")
			("nocommit,n",				po::bool_switch(&nocommit)->implicit_value(true),							"don't commit after writing")
			("noreset,N",				po::bool_switch(&noreset)->implicit_value(true),							"don't reset after commit")
			("notemp,T",				po::bool_switch(&notemp)->implicit_value(true),								"don't commit temporarily, commit to flash")
			("dontwait,d",				po::bool_switch(&option_dontwait)->implicit_value(true),					"don't wait for reply on message")
			("image_slot,x",			po::value<int>(&image_slot)->default_value(-1),								"send image to flash slot x instead of frame buffer")
			("image_timeout,y",			po::value<int>(&image_timeout)->default_value(5000),						"freeze frame buffer for y ms after sending")
			("no-provide-checksum,1",	po::bool_switch(&option_no_provide_checksum)->implicit_value(true),			"do not provide checksum")
			("no-request-checksum,2",	po::bool_switch(&option_no_request_checksum)->implicit_value(true),			"do not request checksum")
			("raw,r",					po::bool_switch(&option_raw)->implicit_value(true),							"do not use packet encapsulation")
			("broadcast-groups,g",		po::value<unsigned int>(&option_broadcast_group_mask)->default_value(0),	"select broadcast groups (bitfield)")
			("burst,u",					po::value<unsigned int>(&option_multicast_burst)->default_value(1),			"burst broadcast and multicast packets multiple times");

		po::positional_options_description positional_options;
		positional_options.add("host", -1);

		po::variables_map varmap;
		auto parsed = po::command_line_parser(argc, argv).options(options).positional(positional_options).run();
		po::store(parsed, varmap);
		po::notify(varmap);

		auto it = host_args.begin();
		host = *(it++);
		auto it1 = it;

		for(; it != host_args.end(); it++)
		{
			if(it != it1)
				args.append(" ");

			args.append(*it);
		}

		selected = 0;

		if(cmd_read)
			selected++;

		if(cmd_write)
			selected++;

		if(cmd_simulate)
			selected++;

		if(cmd_verify)
			selected++;

		if(cmd_benchmark)
			selected++;

		if(cmd_image)
			selected++;

		if(cmd_image_epaper)
			selected++;

		if(cmd_info)
			selected++;

		if(cmd_broadcast)
			selected++;

		if(cmd_multicast)
			selected++;

		if(selected > 1)
			throw(hard_exception("specify one of write/simulate/verify/image/epaper-image/read/info"));

		Espif espif(host, command_port,
					option_verbose, option_debug, cmd_broadcast, cmd_multicast, option_use_tcp, option_raw,
					option_no_provide_checksum, option_no_request_checksum, option_dontwait,
					option_broadcast_group_mask, option_multicast_burst);

		if(selected == 0)
			std::cout << espif.send(args);
		else
		{
			if(cmd_broadcast || cmd_multicast)
				std::cout << espif.multicast(args);
			else
			{
				start = -1;

				try
				{
					start = std::stoi(start_string, 0, 0);
				}
				catch(const std::invalid_argument &)
				{
					throw(hard_exception("invalid value for start argument"));
				}
				catch(const std::out_of_range &)
				{
					throw(hard_exception("invalid value for start argument"));
				}

				try
				{
					length = std::stoi(length_string, 0, 0);
				}
				catch(const std::invalid_argument &)
				{
					throw(hard_exception("invalid value for length argument"));
				}
				catch(const std::out_of_range &)
				{
					throw(hard_exception("invalid value for length argument"));
				}

				std::string reply;
				std::vector<int> int_value;
				std::vector<std::string> string_value;
				unsigned int flash_slot, flash_address[2];

				try
				{
					espif.process("flash-info", "", reply, nullptr,
							"OK flash function available, slots: 2, current: ([0-9]+), sectors: \\[ ([0-9]+), ([0-9]+) \\], display: ([0-9]+)x([0-9]+)px@([0-9]+)",
							&string_value, &int_value);
				}
				catch(const espif_exception &e)
				{
					throw(hard_exception(boost::format("flash incompatible image: %s") % e.what()));
				}

				flash_slot = int_value[0];
				flash_address[0] = int_value[1];
				flash_address[1] = int_value[2];
				dim_x = int_value[3];
				dim_y = int_value[4];
				depth = int_value[5];

				if(option_verbose)
					std::cout <<
							boost::format("flash update available, current slot: %u, address[0]: 0x%x (sector %u), address[1]: 0x%x (sector %u), display graphical dimensions: %ux%u px at depth %u") %
							flash_slot % (flash_address[0] * flash_sector_size) % flash_address[0] % (flash_address[1] * flash_sector_size) % flash_address[1] % dim_x % dim_y % depth << std::endl;

				if(start == -1)
				{
					if(cmd_write || cmd_simulate || cmd_verify || cmd_info)
					{
						flash_slot++;

						if(flash_slot >= 2)
							flash_slot = 0;

						start = flash_address[flash_slot];
						otawrite = true;
					}
					else
						if(!cmd_benchmark && !cmd_image && !cmd_image_epaper)
							throw(hard_exception("start address not set"));
				}

				if(cmd_read)
					espif.read(filename, start, length);
				else
					if(cmd_verify)
						espif.verify(filename, start);
					else
						if(cmd_simulate)
							espif.write(filename, start, true, false);
						else
							if(cmd_write)
							{
								espif.write(filename, start, false, otawrite);

								if(otawrite && !nocommit)
									espif.commit_ota(flash_slot, start, !noreset, notemp);
							}
							else
								if(cmd_benchmark)
									espif.benchmark(length);
								else
									if(cmd_image)
										espif.image(image_slot, filename, dim_x, dim_y, depth, image_timeout);
									else
										if(cmd_image_epaper)
											espif.image_epaper(filename);
			}
		}
	}
	catch(const po::error &e)
	{
		std::cerr << std::endl << boost::format("espif: program option exception: %s") % e.what() << std::endl << options;
		return(1);
	}
	catch(const hard_exception &e)
	{
		std::cerr << std::endl << boost::format("espif: error: %s") % e.what() << std::endl;
		return(1);
	}
	catch(const transient_exception &e)
	{
		std::cerr << std::endl << boost::format("espif: transient exception: %s") % e.what() << std::endl;
		return(1);
	}
	catch(const espif_exception &e)
	{
		std::cerr << std::endl << boost::format("espif: unknown generic espif exception: %s") % e.what() << std::endl;
		return(1);
	}
	catch(const std::exception &e)
	{
		std::cerr << std::endl << boost::format("espif: standard exception: %s") % e.what() << std::endl;
		return(1);
	}
	catch(const std::string &e)
	{
		std::cerr << std::endl << boost::format("espif: unknown standard string exception: %s ") % e << std::endl;
		return(1);
	}
	catch(const char *e)
	{
		std::cerr << std::endl << boost::format("espif: unknown string exception: %s") % e << std::endl;
		return(1);
	}
	catch(...)
	{
		std::cerr << std::endl << "espif: unknown exception" << std::endl;
		return(1);
	}

	return(0);
}
