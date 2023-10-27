#ifndef _command_h_
#define _command_h_

#include "generic_socket.h"
#include "util.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

class Command
{
	public:

		Command() = delete;
		Command(const Util &, GenericSocket &,
				bool raw, bool provide_checksum, bool request_checksum,
				bool dontwait, bool debug, bool verbose,
				unsigned int broadcast_group_mask, unsigned int multicast_burst, unsigned int sector_size) noexcept;

		void read(const std::string &filename, int sector, int sectors) const;
		void write(const std::string filename, int sector, bool simulate, bool otawrite) const;
		void verify(const std::string &filename, int sector) const;
		void benchmark(int length) const;
		void image(int image_slot, const std::string &filename,
				unsigned int dim_x, unsigned int dim_y, unsigned int depth, int image_timeout) const;
		void image_epaper(const std::string &filename) const;
		void send(std::string args) const;
		void multicast(const std::string &args);
		void commit_ota(unsigned int flash_slot, unsigned int sector, bool reset, bool notemp) const;

	private:

		const Util &util;
		GenericSocket &channel;
		const bool raw, provide_checksum, request_checksum;
		const bool dontwait, debug, verbose;
		const unsigned int broadcast_group_mask, multicast_burst, sector_size;
		boost::random::mt19937 prn;

		void image_send_sector(int current_sector, const std::string &data,
				unsigned int current_x, unsigned int current_y, unsigned int depth) const;
		void cie_spi_write(const std::string &data, const char *match) const;
		void cie_uc_cmd_data(bool isdata, unsigned int data_value) const;
		void cie_uc_cmd(unsigned int cmd) const;
		void cie_uc_data(unsigned int data) const;
		void cie_uc_data_string(const std::string valuestring) const;
};

#endif
