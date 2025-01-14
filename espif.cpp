#include "espif.h"
#include "packet.h"
#include "exception.h"

#include <dbus-tiny.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <netdb.h>
#include <string>
#include <iostream>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/regex.hpp>
#include <openssl/evp.h>
#include <Magick++.h>

static const char *flash_info_expect = "OK flash function available, slots: 2, current: ([0-9]+), sectors: \\[ ([0-9]+), ([0-9]+) \\], display: ([0-9]+)x([0-9]+)px@([0-9]+)";

enum
{
	sha1_hash_size = 20,
};

Espif::Espif(const EspifConfig &config_in)
	:
		config(config_in),
		channel(config),
		util(channel, config)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	prn.seed(tv.tv_usec);
}

Espif::~Espif()
{
}

void Espif::read(const std::string &filename, int sector, int sectors) const
{
	int file_fd, offset, current, retries;
	struct timeval time_start, time_now;
	std::string send_string;
	std::string operation;
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	EVP_MD_CTX *hash_ctx;
	unsigned int hash_size;
	unsigned char hash[sha1_hash_size];
	std::string sha_local_hash_text;
	std::string sha_remote_hash_text;
	std::string data;

	if(filename.empty())
		throw(hard_exception("file name required"));

	if((file_fd = open(filename.c_str(), O_WRONLY | O_TRUNC | O_CREAT, 0666)) < 0)
		throw(hard_exception("can't create file"));

	try
	{
		gettimeofday(&time_start, 0);

		if(config.debug)
			std::cout << boost::format("start read from 0x%x (%u), length 0x%x (%u)") % (sector * config.sector_size) % sector % (sectors * config.sector_size) % sectors << std::endl;

		hash_ctx = EVP_MD_CTX_new();
		EVP_DigestInit_ex(hash_ctx, EVP_sha1(), (ENGINE *)0);

		retries = 0;

		for(current = sector, offset = 0; current < (sector + sectors); current++)
		{
			retries += util.read_sector(config.sector_size, current, data);

			if(::write(file_fd, data.data(), data.length()) <= 0)
				throw(hard_exception("i/o error in write"));

			EVP_DigestUpdate(hash_ctx, (const unsigned char *)data.data(), data.length());

			offset += data.length();

			int seconds, useconds;
			double duration, rate;

			gettimeofday(&time_now, 0);

			seconds = time_now.tv_sec - time_start.tv_sec;
			useconds = time_now.tv_usec - time_start.tv_usec;
			duration = seconds + (useconds / 1000000.0);
			rate = offset / 1024.0 / duration;

			std::cout << boost::format("received %3d kbytes in %2.0f seconds at rate %3.0f kbytes/s, received %3u sectors, retries %2u, %3u%%    \r") %
					(offset / 1024) % duration % rate % (current - sector) % retries % ((offset * 100) / (sectors * config.sector_size));
			std::cout.flush();
		}
	}
	catch(...)
	{
		std::cout << std::endl;

		close(file_fd);
		throw;
	}

	close(file_fd);

	std::cout << boost::format("checksumming %u sectors from %u...") % sectors % sector << std::endl;

	hash_size = sha1_hash_size;
	EVP_DigestFinal_ex(hash_ctx, hash, &hash_size);
	EVP_MD_CTX_free(hash_ctx);

	sha_local_hash_text = Util::sha1_hash_to_text(sha1_hash_size, hash);
	util.get_checksum(sector, sectors, sha_remote_hash_text);

	if(sha_local_hash_text != sha_remote_hash_text)
	{
		if(config.verbose)
			std::cout << boost::format("! sector %u / %u, address: 0x%x/0x%x read, checksum failed. Local hash: %s, remote hash: %s") %
					sector % sectors % (sector * config.sector_size) % (sectors * config.sector_size) % sha_local_hash_text % sha_remote_hash_text << std::endl;

		throw(hard_exception("checksum read failed"));
	}

	std::cout << "checksum OK" << std::endl;
}

void Espif::write(const std::string filename, int sector, bool simulate, bool otawrite) const
{
	int file_fd, length, current, offset, retries;
	struct timeval time_start, time_now;
	std::string command;
	std::string send_string;
	std::string reply;
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	EVP_MD_CTX *hash_ctx;
	unsigned int hash_size;
	unsigned char hash[sha1_hash_size];
	std::string sha_local_hash_text;
	std::string sha_remote_hash_text;
	std::string data;
	unsigned int sectors_written, sectors_skipped, sectors_erased;
	unsigned char sector_buffer[config.sector_size];
	struct stat stat;

	if(filename.empty())
		throw(hard_exception("file name required"));

	if((file_fd = open(filename.c_str(), O_RDONLY, 0)) < 0)
		throw(hard_exception("file not found"));

	fstat(file_fd, &stat);
	length = (stat.st_size + (config.sector_size - 1)) / config.sector_size;

	sectors_skipped = 0;
	sectors_erased = 0;
	sectors_written = 0;
	offset = 0;

	try
	{
		gettimeofday(&time_start, 0);

		if(simulate)
			command = "simulate";
		else
		{
			if(otawrite)
				command = "ota ";
			else
				command = "normal ";

			command += "write";
		}

		std::cout << boost::format("start %s at address 0x%06x (sector %u), length: %u (%u sectors)") %
				command % (sector * config.sector_size) % sector % (length * config.sector_size) % length << std::endl;

		hash_ctx = EVP_MD_CTX_new();
		EVP_DigestInit_ex(hash_ctx, EVP_sha1(), (ENGINE *)0);

		retries = 0;

		for(current = sector; current < (sector + length); current++)
		{
			memset(sector_buffer, 0xff, config.sector_size);

			if((::read(file_fd, sector_buffer, config.sector_size)) <= 0)
				throw(hard_exception("i/o error in read"));

			EVP_DigestUpdate(hash_ctx, sector_buffer, config.sector_size);

			retries += util.write_sector(current, std::string((const char *)sector_buffer, sizeof(sector_buffer)),
					sectors_written, sectors_erased, sectors_skipped, simulate);

			offset += config.sector_size;

			int seconds, useconds;
			double duration, rate;

			gettimeofday(&time_now, 0);

			seconds = time_now.tv_sec - time_start.tv_sec;
			useconds = time_now.tv_usec - time_start.tv_usec;
			duration = seconds + (useconds / 1000000.0);
			rate = offset / 1024.0 / duration;

			std::cout << boost::format("sent %4u kbytes in %2.0f seconds at rate %3.0f kbytes/s, sent %3u sectors, written %3u sectors, erased %3u sectors, skipped %3u sectors, retries %2u, %3u%%     \r") %
					(offset / 1024) % duration % rate % (current - sector + 1) % sectors_written % sectors_erased % sectors_skipped % retries %
					(((offset + config.sector_size) * 100) / (length * config.sector_size));
			std::cout.flush();
		}
	}
	catch(...)
	{
		std::cout << std::endl;

		close(file_fd);
		throw;
	}

	close(file_fd);

	std::cout << std::endl;

	if(simulate)
		std::cout << "simulate finished" << std::endl;
	else
	{
		std::cout << boost::format("checksumming %u sectors...") % length << std::endl;

		hash_size = sha1_hash_size;
		EVP_DigestFinal_ex(hash_ctx, hash, &hash_size);
		EVP_MD_CTX_free(hash_ctx);

		sha_local_hash_text = Util::sha1_hash_to_text(sha1_hash_size, hash);

		util.get_checksum(sector, length, sha_remote_hash_text);

		if(sha_local_hash_text != sha_remote_hash_text)
			throw(hard_exception(boost::format("checksum failed: SHA hash differs, local: %u, remote: %s") % sha_local_hash_text % sha_remote_hash_text));

		std::cout << "checksum OK" << std::endl;
		std::cout << "write finished" << std::endl;
	}
}

void Espif::verify(const std::string &filename, int sector) const
{
	int file_fd, offset;
	int current, sectors;
	struct timeval time_start, time_now;
	std::string send_string;
	std::string operation;
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	std::string local_data, remote_data;
	struct stat stat;
	uint8_t sector_buffer[config.sector_size];
	int retries;

	if(filename.empty())
		throw(hard_exception("file name required"));

	if((file_fd = open(filename.c_str(), O_RDONLY)) < 0)
		throw(hard_exception("can't open file"));

	fstat(file_fd, &stat);
	sectors = (stat.st_size + (config.sector_size - 1)) / config.sector_size;
	offset = 0;

	try
	{
		gettimeofday(&time_start, 0);

		if(config.debug)
			std::cout << boost::format("start verify from 0x%x (%u), length 0x%x (%u)") % (sector * config.sector_size) % sector % (sectors * config.sector_size) % sectors << std::endl;

		retries = 0;

		for(current = sector; current < (sector + sectors); current++)
		{
			memset(sector_buffer, 0xff, config.sector_size);

			if(::read(file_fd, sector_buffer, sizeof(sector_buffer)) <= 0)
				throw(hard_exception("i/o error in read"));

			local_data.assign((const char *)sector_buffer, sizeof(sector_buffer));

			retries += util.read_sector(config.sector_size, current, remote_data);

			if(local_data != remote_data)
				throw(hard_exception(boost::format("data mismatch, sector %u") % current));

			offset += sizeof(sector_buffer);

			int seconds, useconds;
			double duration, rate;

			gettimeofday(&time_now, 0);

			seconds = time_now.tv_sec - time_start.tv_sec;
			useconds = time_now.tv_usec - time_start.tv_usec;
			duration = seconds + (useconds / 1000000.0);
			rate = offset / 1024.0 / duration;

			std::cout << boost::format("received %3u kbytes in %2.0f seconds at rate %3.0f kbytes/s, received %3u sectors, retries %2u, %3u%%     \r") %
					(offset / 1024) % duration % rate % (current - sector) % retries % ((offset * 100) / (sectors * config.sector_size));
			std::cout.flush();
		}
	}
	catch(...)
	{
		std::cout << std::endl;
		close(file_fd);
		throw;
	}

	close(file_fd);

	std::cout << std::endl << "verify OK" << std::endl;
}

void Espif::benchmark(int length) const
{
	unsigned int phase, retries, iterations, current;
	std::string command;
	std::string data(config.sector_size, '\0');
	std::string expect;
	std::string reply;
	struct timeval time_start, time_now;
	int seconds, useconds;
	double duration, rate;

	iterations = 1024;

	for(phase = 0; phase < 2; phase++)
	{
		retries = 0;

		gettimeofday(&time_start, 0);

		for(current = 0; current < iterations; current++)
		{
			if(phase == 0)
				retries += util.process("flash-bench 0",
						data,
						reply,
						nullptr,
						"OK flash-bench: sending 0 bytes");
			else
				retries += util.process((boost::format("flash-bench %u") % length).str(),
						"",
						reply,
						&data,
						(boost::format("OK flash-bench: sending %u bytes") % length).str().c_str());

			if(!config.debug)
			{
				gettimeofday(&time_now, 0);

				seconds = time_now.tv_sec - time_start.tv_sec;
				useconds = time_now.tv_usec - time_start.tv_usec;
				duration = seconds + (useconds / 1000000.0);
				rate = current * 4.0 / duration;

				std::cout << boost::format("%s %4u kbytes in %2.0f seconds at rate %3.0f kbytes/s, sent %04u sectors, retries %2u, %3u%%     \r") %
						((phase == 0) ? "sent     " : "received ") % (current * config.sector_size / 1024) % duration % rate % (current + 1) % retries % (((current + 1) * 100) / iterations);
				std::cout.flush();
			}
		}

		usleep(200000);
		std::cout << std::endl;
	}
}

void Espif::image_send_sector(int current_sector, const std::string &data,
		unsigned int current_x, unsigned int current_y, unsigned int depth) const
{
	std::string command;
	std::string reply;
	unsigned int pixels;

	if(current_sector < 0)
	{
		switch(depth)
		{
			case(1):
			{
				pixels = data.length() * 8;
				break;
			}

			case(16):
			{
				pixels = data.length() / 2;
				break;
			}

			case(24):
			{
				pixels = data.length() / 3;
				break;
			}

			default:
			{
				throw(hard_exception("unknown display colour depth"));
			}
		}

		command = (boost::format("display-plot %u %u %u\n") % pixels % current_x % current_y).str();
		util.process(command, data, reply, nullptr, "display plot success: yes");
	}
	else
	{
		unsigned int sectors_written, sectors_erased, sectors_skipped;
		std::string pad;
		unsigned int pad_length = 4096 - data.length();

		pad.assign(pad_length, 0x00);

		util.write_sector(current_sector, data + pad, sectors_written, sectors_erased, sectors_skipped, false);
	}
}

void Espif::image(int image_slot, const std::string &filename,
		unsigned int dim_x, unsigned int dim_y, unsigned int depth, int image_timeout) const
{
	struct timeval time_start, time_now;
	int current_sector;

	gettimeofday(&time_start, 0);

	if(image_slot == 0)
		current_sector = 0x200000 / config.sector_size;
	else
		if(image_slot == 1)
			current_sector = 0x280000 / config.sector_size;
		else
			current_sector = -1;

	try
	{
		Magick::InitializeMagick(nullptr);

		Magick::Image image;
		Magick::Geometry newsize(dim_x, dim_y);
		Magick::Color colour;
		const Magick::Quantum *pixel_cache;

		std::string reply;
		unsigned char sector_buffer[config.sector_size];
		unsigned int start_x, start_y;
		unsigned int current_buffer, x, y;
		double r, g, b;
		int seconds, useconds;
		double duration, rate;

		newsize.aspect(true);

		if(!filename.length())
			throw(hard_exception("empty file name"));

		image.read(filename);

		image.type(MagickCore::TrueColorType);

		if(config.debug)
			std::cout << boost::format("image loaded from %s, %ux%u, version %s") % filename % image.columns() % image.rows() % image.magick() << std::endl;

		image.filterType(Magick::TriangleFilter);
		image.resize(newsize);

		if((image.columns() != dim_x) || (image.rows() != dim_y))
			throw(hard_exception("image magic resize failed"));

		image.modifyImage();

		pixel_cache = image.getPixels(0, 0, dim_x, dim_y);

		if(image_slot < 0)
			util.process((boost::format("display-freeze %u") % 10000).str(), "", reply, nullptr,
					"display freeze success: yes");

		current_buffer = 0;
		start_x = 0;
		start_y = 0;

		memset(sector_buffer, 0xff, config.sector_size);

		for(y = 0; y < dim_y; y++)
		{
			for(x = 0; x < dim_x; x++)
			{
				r = pixel_cache[(((y * dim_x) + x) * 3) + 0] / (1 << MAGICKCORE_QUANTUM_DEPTH);
				g = pixel_cache[(((y * dim_x) + x) * 3) + 1] / (1 << MAGICKCORE_QUANTUM_DEPTH);
				b = pixel_cache[(((y * dim_x) + x) * 3) + 2] / (1 << MAGICKCORE_QUANTUM_DEPTH);

				switch(depth)
				{
					case(1):
					{
						if((current_buffer / 8) + 1 > config.sector_size)
						{
							image_send_sector(current_sector, std::string((const char *)sector_buffer, current_buffer / 8), start_x, start_y, depth);
							memset(sector_buffer, 0xff, config.sector_size);
							current_buffer -= (current_buffer / 8) * 8;
						}

						if((r + g + b) > (3 / 2))
							sector_buffer[current_buffer / 8] |=  (1 << (7 - (current_buffer % 8)));
						else
							sector_buffer[current_buffer / 8] &= ~(1 << (7 - (current_buffer % 8)));

						current_buffer++;

						break;
					}

					case(16):
					{
						unsigned int ru16, gu16, bu16;
						unsigned int r1, g1, g2, b1;

						if((current_buffer + 2) > config.sector_size)
						{
							image_send_sector(current_sector, std::string((const char *)sector_buffer, current_buffer), start_x, start_y, depth);
							memset(sector_buffer, 0xff, config.sector_size);

							if(current_sector >= 0)
								current_sector++;

							current_buffer = 0;
							start_x = x;
							start_y = y;
						}

						ru16 = r * ((1 << 5) - 1);
						gu16 = g * ((1 << 6) - 1);
						bu16 = b * ((1 << 5) - 1);

						r1 = (ru16 & 0b00011111) >> 0;
						g1 = (gu16 & 0b00111000) >> 3;
						g2 = (gu16 & 0b00000111) >> 0;
						b1 = (bu16 & 0b00011111) >> 0;

						sector_buffer[current_buffer++] = (r1 << 3) | (g1 >> 0);
						sector_buffer[current_buffer++] = (g2 << 5) | (b1 >> 0);

						break;
					}

					case(24):
					{
						if((current_buffer + 3) > config.sector_size)
						{
							image_send_sector(current_sector, std::string((const char *)sector_buffer, current_buffer), start_x, start_y, depth);
							memset(sector_buffer, 0xff, config.sector_size);

							if(current_sector >= 0)
								current_sector++;

							current_buffer = 0;
							start_x = x;
							start_y = y;
						}

						sector_buffer[current_buffer++] = r * ((1 << 8) - 1);
						sector_buffer[current_buffer++] = g * ((1 << 8) - 1);
						sector_buffer[current_buffer++] = b * ((1 << 8) - 1);

						break;
					}
				}
			}

			gettimeofday(&time_now, 0);

			seconds = time_now.tv_sec - time_start.tv_sec;
			useconds = time_now.tv_usec - time_start.tv_usec;
			duration = seconds + (useconds / 1000000.0);
			rate = (x * 2 * y) / 1024.0 / duration;

			std::cout << boost::format("sent %4u kbytes in %2.0f seconds at rate %3.0f kbytes/s, x %3u, y %3u, %3u%%    \r") %
					((x * 2 * y) / 1024) % duration % rate % x % y % ((x * y * 100) / (dim_x * dim_y));
			std::cout.flush();
		}

		if(current_buffer > 0)
		{
			if(depth == 1)
			{
				if(current_buffer % 8)
					current_buffer += 8;

				current_buffer /= 8;
			}

			image_send_sector(current_sector, std::string((const char *)sector_buffer, current_buffer), start_x, start_y, depth);
		}

		std::cout << std::endl;

		if(image_slot < 0)
			util.process((boost::format("display-freeze %u") % 0).str(), "", reply, nullptr,
					"display freeze success: yes");

		if((image_slot < 0) && (image_timeout > 0))
			util.process((boost::format("display-freeze %u") % image_timeout).str(), "", reply, nullptr,
					"display freeze success: yes");
	}
	catch(const Magick::Error &error)
	{
		throw(hard_exception(boost::format("image: load failed: %s") % error.what()));
	}
	catch(const Magick::Warning &warning)
	{
		std::cout << boost::format("image: %s") % warning.what() << std::endl;
	}
}

Espif::ProxyThread::ProxyThread(Espif &espif_in, const std::vector<std::string> &signal_ids_in)
	: espif(espif_in), signal_ids(signal_ids_in)
{
}

void Espif::ProxyThread::operator()()
{
	std::string message_type;
	std::string message_interface;
	std::string message_method;
	std::string reply;
	std::string time_string;
	std::string service = (boost::format("%s.%s") % dbus_service_id % espif.config.host).str();

	try
	{
		DbusTinyServer dbus_tiny_server(service);

		for(const auto &it : signal_ids)
		{
			std::cerr << "registering signal: " << it << std::endl;
			dbus_tiny_server.register_signal((boost::format("%s.%s.%s") % dbus_service_id % "signal" % it).str());
		}

		for(;;)
		{
			try
			{
				dbus_tiny_server.get_message(message_type, message_interface, message_method);

				if(message_type == "method call")
				{
					std::cerr << boost::format("message received, interface: %s, method: %s\n") % message_interface % message_method;

					if(message_interface == "org.freedesktop.DBus.Introspectable")
					{
						if(message_method == "Introspect")
						{
							reply += std::string("") +
											"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n" +
											"<node>\n" +
											"	<interface name=\"org.freedesktop.DBus.Introspectable\">\n" +
											"		<method name=\"Introspect\">\n" +
											"			<arg name=\"xml\" type=\"s\" direction=\"out\"/>\n" +
											"		</method>\n" +
											"	</interface>\n" +
											"	<interface name=\"" + service + "\">\n" +
											"		<method name=\"dump\">\n" +
											"			<arg name=\"info\" type=\"s\" direction=\"out\"/>\n" +
											"		</method>\n" +
											"		<method name=\"get_sensor_data\">\n" +
											"			<arg name=\"module\" type=\"u\" direction=\"in\"/>\n" +
											"			<arg name=\"bus\" type=\"u\" direction=\"in\"/>\n" +
											"			<arg name=\"name\" type=\"s\" direction=\"in\"/>\n" +
											"			<arg name=\"type\" type=\"s\" direction=\"in\"/>\n" +
											"			<arg name=\"time\" type=\"t\" direction=\"out\"/>\n" +
											"			<arg name=\"id\" type=\"u\" direction=\"out\"/>\n" +
											"			<arg name=\"address\" type=\"u\" direction=\"out\"/>\n" +
											"			<arg name=\"unity\" type=\"s\" direction=\"out\"/>\n" +
											"			<arg name=\"value\" type=\"d\" direction=\"out\"/>\n" +
											"		</method>\n" +
											"		<method name=\"get_uart_data\">\n" +
											"			<arg name=\"data\" type=\"s\" direction=\"out\"/>\n" +
											"		</method>\n" +
											"		<method name=\"push_command\">\n" +
											"			<arg name=\"command\" type=\"s\" direction=\"in\"/>\n" +
											"			<arg name=\"status\" type=\"s\" direction=\"out\"/>\n" +
											"		</method>\n" +
											"	</interface>\n";

							for(const auto &it : signal_ids)
							{
								reply += std::string("") +
											"	<interface name=\"" + dbus_service_id + ".signal." + it + "\">\n" +
											"		<signal name=\"push_command\">\n" +
											"			<arg name=\"command\" type=\"s\"/>\n" +
											"		</signal>\n" +
											"	</interface>\n";
							}

							reply += "</node>\n";

							dbus_tiny_server.send_string(reply);
							reply.clear();
						}
						else
							throw(transient_exception(dbus_tiny_server.inform_error(std::string("unknown introspection method called"))));
					}
					else if((message_interface == dbus_service_id) || (message_interface == ""))
					{
						if(message_method == "dump")
						{
							reply += "SENSOR DATA\n\n";

							for(const auto &it : espif.proxy_sensor_data)
							{
								Util::time_to_string(time_string, it.second.time);

								reply += (boost::format("> %1u %1u %-16s %-16s / %2u @ %02x %8.2f %-3s %s\n") %
											it.first.module % it.first.bus % it.first.name % it.first.type %
											it.second.id % it.second.address % it.second.value % it.second.unity % time_string).str();
							}

							reply += "\nCOMMANDS\n\n";

							for(const auto &it : espif.proxy_commands)
							{
								Util::time_to_string(time_string, it.time);

								reply += (boost::format("> %s from %s at %s\n") % it.command % it.source % time_string).str();
							}

							dbus_tiny_server.send_string(reply);
							reply.clear();
						}
						else if(message_method == "get_sensor_data")
						{
							unsigned int module;
							unsigned int bus;
							std::string name;
							std::string type;
							ProxySensorDataKey key;
							ProxySensorData::const_iterator it;

							dbus_tiny_server.receive_uint32_uint32_string_string(module, bus, name, type);

							key.module = module;
							key.bus = bus;
							key.name = name;
							key.type = type;

							if((it = espif.proxy_sensor_data.find(key)) == espif.proxy_sensor_data.end())
								throw(transient_exception(dbus_tiny_server.inform_error((boost::format("not found: %u/%u/%s/%s") % key.module % key.bus % key.name % key.type).str())));

							dbus_tiny_server.send_uint64_uint32_uint32_string_double(it->second.time, it->second.id, it->second.address, it->second.unity, it->second.value);
						}
						else if(message_method == "get_uart_data")
						{
							std::string data = espif.uart_data;
							espif.uart_data.clear();
							dbus_tiny_server.send_string(data);
						}
						else if(message_method == "push_command")
						{
							std::string command;
							ProxyCommandEntry entry;

							command = dbus_tiny_server.receive_string();

							entry.time = time((time_t *)0);
							entry.source = "message";
							entry.command = command;

							espif.proxy_commands.push_back(entry);

							dbus_tiny_server.send_string("ok");
						}
						else
							throw(transient_exception(dbus_tiny_server.inform_error(std::string("unknown method called"))));
					}
					else
						throw(transient_exception(dbus_tiny_server.inform_error((boost::format("message not for our interface: %s") % message_interface).str())));
				}
				else if(message_type == "signal")
				{
					std::cerr << boost::format("signal received, interface: %s, method: %s\n") % message_interface % message_method;

					if(message_interface == "org.freedesktop.DBus")
					{
						if(message_method == "NameAcquired")
							std::cerr << "name on dbus acquired\n";
					}
					else
					{
						std::vector<std::string>::const_iterator it;

						for(it = signal_ids.begin(); it != signal_ids.end(); it++)
						{
							std::string interface_check = (boost::format("%s.%s.%s") % dbus_service_id % "signal" % *it).str();

							if(message_interface == interface_check)
								break;
						}

						if(it == signal_ids.end())
							throw(transient_exception(dbus_tiny_server.inform_error(std::string("signal to unknown interface received"))));

						if(message_method == "push_command")
						{
							std::string command;
							ProxyCommandEntry entry;

							command = dbus_tiny_server.receive_string();

							entry.time = time((time_t *)0);
							entry.source = "signal";
							entry.command = command;

							espif.proxy_commands.push_back(entry);
						}
						else
							throw(transient_exception(dbus_tiny_server.inform_error(std::string("unknown signal received"))));
					}
				}
				else
					throw(transient_exception(boost::format("message of unknown type: %u") % message_type));
			}
			catch(const DbusTinyException &e)
			{
				std::cerr << boost::format("libdbus-tiny warning: %s\n") % e.what();
			}
			catch(const transient_exception &e)
			{
				std::cerr << boost::format("warning: %s\n") % e.what();
			}

			dbus_tiny_server.reset();
		}
	}
	catch(const hard_exception &e)
	{
		std::cerr << "espif proxy: fatal: " << e.what() << std::endl;
		exit(1);
	}
}

void Espif::run_proxy(bool read_uart, const std::vector<std::string> &signal_ids)
{
	std::string command, reply, line, time_string;
	unsigned int pos;
	ProxySensorDataKey key;
	ProxySensorDataEntry data;
	boost::smatch capture;
	boost::regex re("sensor ([0-9]+)/([0-9]+)@([0-9a-fA-F]+): +([^,]+), +([^:]+): +[[]([0-9.U-]+)[]] +([a-zA-Z%]+)?");
	struct ProxyCommandEntry entry;

	proxy_thread_class = new ProxyThread(*this, signal_ids);
	boost::thread proxy_thread(*proxy_thread_class);
	proxy_thread.detach();

	for(;;)
	{
		boost::this_thread::sleep_for(boost::chrono::duration<unsigned int>(10));

		command = "isd";

		if(!channel.send(command, 10000))
		{
			std::cerr << "sensor data send timeout\n";
			continue;
		}

		reply.clear();

		if(!channel.receive(reply, 10000))
		{
			std::cerr << "sensor receive timeout\n";
			continue;
		}

		while(reply.length() > 0)
		{
			if((pos = reply.find('\n')) == std::string::npos)
			{
				line = reply;
				reply.clear();
			}
			else
			{
				line = reply.substr(0, pos);
				reply.erase(0, pos + 1);
			}

			if(!boost::regex_match(line, capture, re))
			{
				//std::cerr << "sensor data: no match: " << line << "\n";
				continue;
			}

			if(capture.size() < 8)
			{
				std::cerr << "partial match\n";
				continue;
			}

			try
			{
				data.time = time((time_t *)0);
				key.module = 0;
				key.bus = (unsigned int)stoi(capture[1]);
				data.id = (unsigned int)stoi(capture[2]);
				data.address = (unsigned int)stoi(capture[3], nullptr, 16);
				key.name = capture[4];
				key.type = capture[5];
				data.value = stod(capture[6]);
				data.unity = capture[7];
			}
			catch(std::invalid_argument &)
			{
				std::cerr << "error in integer conversion\n";
				continue;
			}
			catch(std::out_of_range &)
			{
				std::cerr << "error in integer conversion\n";
				continue;
			}

			proxy_sensor_data[key] = data;
		}

		if(read_uart)
		{
			command = "ur";

			if(!channel.send(command, 10000))
			{
				std::cerr << "uart data send timeout\n";
				continue;
			}

			reply.clear();

			if(!channel.receive(reply, 10000))
			{
				std::cerr << "uart receive timeout\n";
				continue;
			}

			if(reply.length() > 0)
				uart_data = reply;
		}

		Espif::ProxyCommands::iterator it;

		for(it = proxy_commands.begin(); it != proxy_commands.end(); it++)
		{
			if((time((time_t *)0) - it->time) > (5 * 60))
			{
				std::cerr << "erasing timed out command: " << it->command << ", timestamp: " << it->time << std::endl;
				proxy_commands.erase(it);
				it = proxy_commands.begin();
			}
		}

		if(proxy_commands.size() > 0)
		{
			while(proxy_commands.size() > 0)
			{
				entry = proxy_commands.front();

				if(!channel.send(entry.command, 1000))
				{
					std::cerr << "command send timeout\n";
					boost::this_thread::sleep_for(boost::chrono::duration<unsigned int>(1));
					break;
				}

				reply.clear();

				if(!channel.receive(reply, 1000))
				{
					std::cerr << "command receive timeout\n";
					boost::this_thread::sleep_for(boost::chrono::duration<unsigned int>(1));
					continue;
				}

				proxy_commands.pop_front();
			}
		}
	}
}

void Espif::cie_spi_write(const std::string &data, const char *match) const
{
	std::string reply;

	util.process(data, "", reply, nullptr, match);
}

void Espif::cie_uc_cmd_data(bool isdata, unsigned int data_value) const
{
	boost::format fmt("spt 17 8 %02x 0 0 0 0");
	std::string reply;

	fmt % data_value;

	cie_spi_write("sps", "spi start ok");
	cie_spi_write(std::string("iw 1 0 ") + (isdata ? "1" : "0"), (std::string("digital output: \\[") + (isdata ? "1" : "0") + "\\]").c_str());
	cie_spi_write(fmt.str(), "spi transmit ok");
	cie_spi_write("spf", "spi finish ok");
}

void Espif::cie_uc_cmd(unsigned int cmd) const
{
	return(cie_uc_cmd_data(false, cmd));
}

void Espif::cie_uc_data(unsigned int data) const
{
	return(cie_uc_cmd_data(true, data));
}

void Espif::cie_uc_data_string(const std::string valuestring) const
{
	cie_spi_write("iw 1 0 1", "digital output: \\[1\\]");
	cie_spi_write("sps", "spi start ok");
	cie_spi_write((boost::format("spw 8 %s") % valuestring).str(), "spi write ok");
	cie_spi_write("spt 17 0 0 0 0 0 0 0", "spi transmit ok");
	cie_spi_write("spf", "spi finish ok");
}

void Espif::image_epaper(const std::string &filename) const
{
	static const unsigned int dim_x = 212;
	static const unsigned int dim_y = 104;
	uint8_t dummy_display[dim_x][dim_y];
	std::vector<int> int_value;
	std::vector<std::string> string_value;
	std::string values, command, reply;
	unsigned int layer, all_bytes, bytes, byte, bit;
	int x, y;
	struct timeval time_start, time_now;
	int seconds, useconds;
	double duration, rate;

	gettimeofday(&time_start, 0);

	cie_spi_write("spc 0 0", "spi configure ok");

	cie_uc_cmd(0x04); 	// power on PON, no arguments

	cie_uc_cmd(0x00);	// panel settings PSR, 1 argument
	cie_uc_data(0x0f);	// default

	cie_uc_cmd(0x61); 	// resultion settings TSR, 3 argument
	cie_uc_data(0x68);	// height
	cie_uc_data(0x00);	// width[7]
	cie_uc_data(0xd4);	// width[6-0]

	cie_uc_cmd(0x50); 	// vcom and data interval setting, 1 argument
	cie_uc_data(0xd7);	// default

	try
	{
		Magick::Image image;
		Magick::Geometry newsize(dim_x, dim_y);
		Magick::Color colour;
		newsize.aspect(true);

		if(!filename.length())
			throw(hard_exception("image epaper: empty file name"));

		image.read(filename);

		if(config.debug)
			std::cout << boost::format("image loaded from %s, %ux%u, version: %s") % filename % image.columns() % image.rows() % image.magick() << std::endl;

		image.resize(newsize);

		if((image.columns() != dim_x) || (image.rows() != dim_y))
			throw(hard_exception("image epaper: image magic resize failed"));

		all_bytes = 0;
		bytes = 0;
		byte = 0;
		bit = 7;
		values = "";
		cie_spi_write("sps", "spi start ok");

		for(x = 0; x < (int)dim_x; x++)
			for(y = 0; y < (int)dim_y; y++)
				dummy_display[x][y] = 0;

		for(layer = 0; layer < 2; layer++)
		{
			cie_uc_cmd(layer == 0 ? 0x10 : 0x13); // DTM1 / DTM2

			for(x = dim_x - 1; x >= 0; x--)
			{
				for(y = 0; y < (int)dim_y; y++)
				{
					colour = image.pixelColor(x, y);

					if(layer == 0)
					{
						//if((colour.quantumRed() > 16384) && (colour.quantumGreen() > 16384) && (colour.quantumBlue() > 16384)) // FIXME
						//{
							//dummy_display[x][y] |= 0x01;
							//byte |= 1 << bit;
						//}
					}
					else
					{
						//if((colour.quantumRed() > 16384) && (colour.quantumGreen() < 16384) && (colour.quantumBlue() < 16384)) // FIXME
						//{
							//dummy_display[x][y] |= 0x02;
							//byte |= 1 << bit;
						//}
					}

					if(bit > 0)
						bit--;
					else
					{
						values.append((boost::format("%02x ") % byte).str());
						all_bytes++;
						bytes++;
						bit = 7;
						byte = 0;

						if(bytes > 31)
						{
							cie_uc_data_string(values);
							values = "";
							bytes = 0;
						}
					}
				}

				gettimeofday(&time_now, 0);

				seconds = time_now.tv_sec - time_start.tv_sec;
				useconds = time_now.tv_usec - time_start.tv_usec;
				duration = seconds + (useconds / 1000000.0);
				rate = all_bytes / 1024.0 / duration;

				std::cout << boost::format("sent %4u kbytes in %2.0f seconds at rate %3.0f kbytes/s, x %3u, y %3u, %3u%%     \r") %
						(all_bytes / 1024) % duration % rate % x % y % (((dim_x - 1 - x) * y * 100) / (2 * dim_x * dim_y));
				std::cout.flush();
			}

			if(bytes > 0)
			{
				cie_uc_data_string(values);
				values = "";
				bytes = 0;
			}

			cie_uc_cmd(0x11); // data stop DST
		}

		cie_uc_cmd(0x12); // display refresh DRF
	}
	catch(const Magick::Error &e)
	{
		throw(hard_exception(boost::format("image epaper: load failed: %s") % e.what()));
	}
	catch(const Magick::Warning &e)
	{
		std::cout << boost::format("image epaper: %s") % e.what() << std::endl;
	}

	if(config.debug)
	{
		for(y = 0; y < 104; y++)
		{
			for(x = 0; x < 200; x++)
			{
				switch(dummy_display[x][y])
				{
					case(0): fputs(" ", stdout); break;
					case(1): fputs("1", stdout); break;
					case(2): fputs("2", stdout); break;
					default: fputs("*", stdout); break;
				}
			}

			fputs("$\n", stdout);
		}
	}
}

std::string Espif::send(std::string args) const
{
	std::string arg;
	size_t current;
	Packet send_packet;
	std::string send_data;
	Packet receive_packet;
	std::string receive_data;
	std::string reply;
	std::string reply_oob;
	std::string output;
	int retries;

	if(config.dontwait)
	{
		if(daemon(0, 0))
		{
			perror("daemon");
			return(std::string(""));
		}
	}

	while(args.length() > 0)
	{
		if((current = args.find('\n')) != std::string::npos)
		{
			arg = args.substr(0, current);
			args.erase(0, current + 1);
		}
		else
		{
			arg = args;
			args.clear();
		}

		retries = util.process(arg, "", reply, &reply_oob);

		output.append(reply);

		if(reply_oob.length() > 0)
		{
			unsigned int length = 0;

			output.append((boost::format("\n%u bytes of OOB data: ") % reply_oob.length()).str());

			for(const auto &it : reply_oob)
			{
				if((length++ % 20) == 0)
					output.append("\n    ");

				output.append((boost::format("0x%02x ") % (((unsigned int)it) & 0xff)).str());
			}

		}

		output.append("\n");

		if((retries > 0) && config.verbose)
			std::cout << boost::format("%u retries\n") % retries;
	}

	return(output);
}

std::string Espif::multicast(const std::string &args)
{
	Packet send_packet(args);
	Packet receive_packet;
	std::string send_data;
	std::string receive_data;
	std::string reply_data;
	std::string packet;
	struct timeval tv_start, tv_now;
	uint64_t start, now;
	struct sockaddr_in remote_host;
	char host_buffer[64];
	char service[64];
	std::string hostname;
	int gai_error;
	std::string reply;
	std::string info;
	std::string line;
	uint32_t host_id;
	typedef struct { int count; std::string hostname; std::string text; } multicast_reply_t;
	typedef std::map<unsigned uint32_t, multicast_reply_t> multicast_replies_t;
	multicast_replies_t multicast_replies;
	int total_replies, total_hosts;
	int run;
	uint32_t transaction_id;
	std::string output;

	transaction_id = prn();
	packet = send_packet.encapsulate(config.raw, config.provide_checksum, config.request_checksum, config.broadcast_group_mask, &transaction_id);

	if(config.dontwait)
	{
		for(run = 0; run < (int)config.multicast_burst; run++)
		{
			send_data = packet;
			channel.send(send_data);
			usleep(100000);
		}

		return(std::string(""));
	}

	total_replies = total_hosts = 0;

	gettimeofday(&tv_start, nullptr);
	start = (tv_start.tv_sec * 1000000) + tv_start.tv_usec;

	for(run = 0; run < (int)config.multicast_burst; run++)
	{
		gettimeofday(&tv_now, nullptr);
		now = (tv_now.tv_sec * 1000000) + tv_now.tv_usec;

		if(((now - start) / 1000ULL) > 10000)
			break;

		send_data = packet;
		channel.send(send_data, 100);

		for(;;)
		{
			reply_data.clear();

			if(!channel.receive(reply_data, 100, &remote_host))
				break;

			receive_packet.clear();
			receive_packet.append_data(reply_data);

			if(!receive_packet.decapsulate(&reply_data, nullptr, config.verbose, nullptr, &transaction_id))
			{
				if(config.verbose)
					std::cout << "multicast: cannot decapsulate" << std::endl;

				continue;
			}

			host_id = ntohl(remote_host.sin_addr.s_addr);

			gai_error = getnameinfo((struct sockaddr *)&remote_host, sizeof(remote_host), host_buffer, sizeof(host_buffer),
					service, sizeof(service), NI_DGRAM | NI_NUMERICSERV | NI_NOFQDN);

			if(gai_error != 0)
			{
				if(config.verbose)
					std::cout << boost::format("cannot resolve: %s") % gai_strerror(gai_error) << std::endl;

				hostname = "0.0.0.0";
			}
			else
				hostname = host_buffer;

			total_replies++;

			auto it = multicast_replies.find(host_id);

			if(it != multicast_replies.end())
				it->second.count++;
			else
			{
				total_hosts++;
				multicast_reply_t entry;

				entry.count = 1;
				entry.hostname = hostname;
				entry.text = reply_data;
				multicast_replies[host_id] = entry;
			}
		}
	}

	for(auto &it : multicast_replies)
	{
		boost::format ip("%u.%u.%u.%u");

		ip % ((it.first & 0xff000000) >> 24) %
				((it.first & 0x00ff0000) >> 16) %
				((it.first & 0x0000ff00) >>  8) %
				((it.first & 0x000000ff) >>  0);

		output.append((boost::format("%-14s %2u %-12s %s\n") % ip % it.second.count % it.second.hostname % it.second.text).str());
	}

	output.append((boost::format("\n%u probes sent, %u replies received, %u hosts\n") % config.multicast_burst % total_replies % total_hosts).str());

	return(output);
}

void Espif::commit_ota(unsigned int flash_slot, unsigned int sector, bool reset, bool notemp)
{
	std::string reply;
	std::vector<std::string> string_value;
	std::vector<int> int_value;
	std::string send_data;
	Packet packet;
	static const char *flash_select_expect = "OK flash-select: slot ([0-9]+) selected, sector ([0-9]+), permanent ([0-1])";

	send_data = (boost::format("flash-select %u %u") % flash_slot % (notemp ? 1 : 0)).str();
	util.process(send_data, "", reply, nullptr, flash_select_expect, &string_value, &int_value);

	if(int_value[0] != (int)flash_slot)
		throw(hard_exception(boost::format("flash-select failed, local slot (%u) != remote slot (%u)") % flash_slot % int_value[0]));

	if(int_value[1] != (int)sector)
		throw(hard_exception("flash-select failed, local sector != remote sector"));

	if(int_value[2] != notemp ? 1 : 0)
		throw(hard_exception("flash-select failed, local permanent != remote permanent"));

	std::cout << boost::format("selected %s boot slot: %u") % (notemp ? "permanent" : "one time") % flash_slot << std::endl;

	if(!reset)
		return;

	std::cout << "rebooting... ";
	std::cout.flush();

	packet.clear();
	packet.append_data("reset\n");
	send_data = packet.encapsulate(config.raw, config.provide_checksum, config.request_checksum, config.broadcast_group_mask);
	channel.send(send_data);
	channel.disconnect();
	channel.connect();
	util.process("flash-info", "", reply, nullptr, flash_info_expect, &string_value, &int_value);
	std::cout << "reboot finished" << std::endl;
	util.process("flash-info", "", reply, nullptr, flash_info_expect, &string_value, &int_value);

	if(int_value[0] != (int)flash_slot)
		throw(hard_exception(boost::format("boot failed, requested slot (%u) != active slot (%u)") % flash_slot % int_value[0]));

	if(!notemp)
	{
		std::cout << boost::format("boot succeeded, permanently selecting boot slot: %u") % flash_slot << std::endl;

		send_data = (boost::format("flash-select %u 1") % flash_slot).str();
		util.process(send_data, "", reply, nullptr, flash_select_expect, &string_value, &int_value);

		if(int_value[0] != (int)flash_slot)
			throw(hard_exception(boost::format("flash-select failed, local slot (%u) != remote slot (%u)") % flash_slot % int_value[0]));

		if(int_value[1] != (int)sector)
			throw(hard_exception("flash-select failed, local sector != remote sector"));

		if(int_value[2] != 1)
			throw(hard_exception("flash-select failed, local permanent != remote permanent"));
	}

	util.process("stats", "", reply, nullptr, "\\s*>\\s*firmware\\s*>\\s*date:\\s*([a-zA-Z0-9: ]+).*", &string_value, &int_value);
	std::cout << boost::format("firmware version: %s") % string_value[0] << std::endl;
}

int Espif::process(const std::string &data, const std::string &oob_data,
				std::string &reply_data, std::string *reply_oob_data,
				const char *match, std::vector<std::string> *string_value, std::vector<int> *int_value) const
{
	return(util.process(data, oob_data, reply_data, reply_oob_data, match, string_value, int_value));
}
