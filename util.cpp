#include "util.h"
#include "packet.h"
#include "exception.h"

#include <string>
#include <iostream>
#include <boost/format.hpp>
#include <boost/regex.hpp>

Util::Util(GenericSocket &channel_in,
		bool verbose_in, bool debug_in, bool raw_in, bool provide_checksum_in, bool request_checksum_in, unsigned int broadcast_group_mask_in) noexcept
	:
		channel(channel_in),
		verbose(verbose_in), debug(debug_in), raw(raw_in),
		provide_checksum(provide_checksum_in), request_checksum(request_checksum_in),
		broadcast_group_mask(broadcast_group_mask_in)
{
}

std::string Util::dumper(const char *id, const std::string text)
{
	int ix;
	char current;
	std::string out;

	out = (boost::format("%s[%d]: \"") % id % text.length()).str();

	for(ix = 0; (ix < (int)text.length()) && (ix < 96); ix++)
	{
		current = text.at(ix);

		if((current >= ' ') && (current <= '~'))
			out.append(1, current);
		else
			out.append((boost::format("[%02x]") % ((unsigned int)current & 0xff)).str());
	}

	out.append("\"");

	return(out);
}

std::string Util::sha1_hash_to_text(unsigned int length, const unsigned char *hash)
{
	unsigned int current;
	std::string hash_string;

	for(current = 0; current < length; current++)
		hash_string.append((boost::format("%02x") % (unsigned int)hash[current]).str());

	return(hash_string);
}

int Util::process(const std::string &data, const std::string &oob_data, std::string &reply_data, std::string *reply_oob_data,
		const char *match, std::vector<std::string> *string_value, std::vector<int> *int_value) const
{
	enum { max_attempts = 4 };
	unsigned int attempt;
	Packet send_packet(data, oob_data);
	std::string send_data;
	std::string packet;
	Packet receive_packet;
	std::string receive_data;
	boost::smatch capture;
	boost::regex re(match ? match : "");
	unsigned int captures;
	int timeout;

	if(debug)
		std::cout << std::endl << Util::dumper("data", data) << std::endl;

	packet = send_packet.encapsulate(raw, provide_checksum, request_checksum, broadcast_group_mask);

	timeout = 200;

	for(attempt = 0; attempt < max_attempts; attempt++)
	{
		try
		{
			send_data = packet;

			while(send_data.length() > 0)
				if(!channel.send(send_data))
					throw(transient_exception("send failed"));

			receive_packet.clear();

			while(!receive_packet.complete())
			{
				receive_data.clear();

				if(!channel.receive(receive_data))
					throw(transient_exception("receive failed"));

				receive_packet.append_data(receive_data);
			}

			if(!receive_packet.decapsulate(&reply_data, reply_oob_data, verbose))
				throw(transient_exception("decapsulation failed"));

			if(match && !boost::regex_match(reply_data, capture, re))
				throw(transient_exception(boost::format("received string does not match: \"%s\" vs. \"%s\"") % Util::dumper("reply", reply_data) % match));

			break;
		}
		catch(const transient_exception &e)
		{
			std::cout << std::endl << boost::format("process attempt #%u failed: %s, backoff %u ms") % attempt % e.what() % timeout << std::endl;

			channel.drain(timeout);
			timeout *= 2;

			continue;
		}
	}

	if(verbose && (attempt > 0))
		std::cerr << std::endl << boost::format("success at attempt %u") % attempt << std::endl;

	if(attempt >= max_attempts)
		throw(hard_exception("process: no more attempts"));

	if(string_value || int_value)
	{
		if(string_value)
			string_value->clear();

		if(int_value)
			int_value->clear();

		captures = 0;

		for(const auto &it : capture)
		{
			if(captures++ == 0)
				continue;

			if(string_value)
				string_value->push_back(it);

			if(int_value)
			{
				try
				{
					int_value->push_back(stoi(it, 0, 0));
				}
				catch(std::invalid_argument &)
				{
					int_value->push_back(0);
				}
				catch(std::out_of_range &)
				{
					int_value->push_back(0);
				}
			}
		}
	}

	if(debug)
		std::cout << std::endl << Util::dumper("reply", reply_data) << std::endl;

	return(attempt);
}

int Util::read_sector(unsigned int sector_size, unsigned int sector, std::string &data) const
{
	std::string reply;
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	int retries;

	try
	{
		retries = process((boost::format("flash-read %u\n") % sector).str(), "",
				reply, &data, "OK flash-read: read sector ([0-9]+)", &string_value, &int_value);
	}
	catch(const hard_exception &e)
	{
		throw(hard_exception(boost::format("read sector: hard exception: %s") % e.what()));
	}
	catch(const transient_exception &e)
	{
		throw(transient_exception(boost::format("read sector: transient exception: %s") % e.what()));
	}

	if(data.length() < sector_size)
	{
		if(verbose)
			std::cout << boost::format("flash sector read failed: incorrect length, expected: %u, received: %u, reply: %s") %
					sector_size % data.length() % reply << std::endl;

		throw(transient_exception(boost::format("read_sector failed: incorrect length (%u vs. %u)") % sector_size % data.length()));
	}

	if(int_value[0] != (int)sector)
	{
		if(verbose)
			std::cout << boost::format("flash sector read failed: local sector #%u != remote sector #%u") % sector % int_value[0] << std::endl;

		throw(transient_exception(boost::format("read sector failed: incorrect sector (%u vs. %u)") % sector % int_value[0]));
	}

	return(retries);
}

int Util::write_sector(unsigned int sector, const std::string &data,
		unsigned int &written, unsigned int &erased, unsigned int &skipped, bool simulate) const
{
	std::string command;
	std::string reply;
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	unsigned int attempt;
	unsigned int process_tries;

	command = (boost::format("flash-write %u %u") % (simulate ? 0 : 1) % sector).str();

	for(attempt = 4; attempt > 0; attempt--)
	{
		try
		{
			process_tries = process(command, data,
					reply, nullptr, "OK flash-write: written mode ([01]), sector ([0-9]+), same ([01]), erased ([01])", &string_value, &int_value);

			if(int_value[0] != (simulate ? 0 : 1))
				throw(transient_exception(boost::format("invalid mode (%u vs. %u)") % (simulate ? 0 : 1) % int_value[0]));

			if(int_value[1] != (int)sector)
				throw(transient_exception(boost::format("wrong sector (%u vs %u)") % sector % int_value[0]));

			if(int_value[2] != 0)
				skipped++;
			else
				written++;

			if(int_value[3] != 0)
				erased++;

			break;
		}
		catch(const transient_exception &e)
		{
			std::cerr << std::endl << boost::format("flash sector write failed temporarily: %s, reply: %s ") % e.what() % reply << std::endl;
			continue;
		}
	}

	if(attempt == 0)
		throw(hard_exception("write sector: no more attempts"));

	return(process_tries);
}

void Util::get_checksum(unsigned int sector, unsigned int sectors, std::string &checksum) const
{
	std::string reply;
	std::vector<int> int_value;
	std::vector<std::string> string_value;

	try
	{
		process((boost::format("flash-checksum %u %u\n") % sector % sectors).str(), "",
				reply, nullptr, "OK flash-checksum: checksummed ([0-9]+) sectors from sector ([0-9]+), checksum: ([0-9a-f]+)",
				&string_value, &int_value);
	}
	catch(const transient_exception &e)
	{
		boost::format fmt("flash sector checksum failed temporarily: %s, reply: %s");

		fmt % e.what() % reply;

		if(verbose)
			std::cout << fmt << std::endl;

		throw(transient_exception(fmt));
	}
	catch(const hard_exception &e)
	{
		boost::format fmt("flash sector checksum failed: %s, reply: %s");

		fmt % e.what() % reply;

		if(verbose)
			std::cout << fmt << std::endl;

		throw(hard_exception(fmt));
	}

	if(int_value[0] != (int)sectors)
	{
		boost::format fmt("flash sector checksum failed: local sectors (%u) != remote sectors (%u)");

		fmt % sectors % int_value[0];

		if(verbose)
			std::cout << fmt << std::endl;

		throw(transient_exception(fmt));
	}

	if(int_value[1] != (int)sector)
	{
		boost::format fmt("flash sector checksum failed: local start sector (%u) != remote start sector (%u)");

		fmt % sector % int_value[1];

		if(verbose)
			std::cout << fmt << std::endl;

		throw(transient_exception(fmt));
	}

	checksum = string_value[2];
}
