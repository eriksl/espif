#ifndef _espif_h_
#define _espif_h_

#include "espifconfig.h"
#include "generic_socket.h"
#include "util.h"

#include <string>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

class Espif
{
	public:

		Espif() = delete;
		Espif(const EspifConfig &);

		void read(const std::string &filename, int sector, int sectors) const;
		void write(const std::string filename, int sector, bool simulate, bool otawrite) const;
		void verify(const std::string &filename, int sector) const;
		void benchmark(int length) const;
		void image(int image_slot, const std::string &filename,
				unsigned int dim_x, unsigned int dim_y, unsigned int depth, int image_timeout) const;
		void image_epaper(const std::string &filename) const;
		std::string send(std::string args) const;
		std::string multicast(const std::string &args);
		void commit_ota(unsigned int flash_slot, unsigned int sector, bool reset, bool notemp);
		int process(const std::string &data, const std::string &oob_data,
				std::string &reply_data, std::string *reply_oob_data,
				const char *match = nullptr, std::vector<std::string> *string_value = nullptr, std::vector<int> *int_value = nullptr) const;

	private:

		const EspifConfig config;
		GenericSocket channel;
		const Util util;
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
