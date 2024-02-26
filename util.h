#ifndef _util_h_
#define _util_h_
#include "generic_socket.h"
#include "util.h"

#include <string>
#include <vector>

class Util
{
	friend class Command;
	friend class Espif;
	friend class GenericSocket;

	protected:

		Util() = delete;
		Util(GenericSocket &channel, bool verbose, bool debug, bool raw, bool provide_checksum, bool request_checksum, unsigned int broadcast_group_mask) noexcept;

		static std::string dumper(const char *id, const std::string text);
		static std::string sha1_hash_to_text(unsigned int length, const unsigned char *hash);

		int process(const std::string &data, const std::string &oob_data,
				std::string &reply_data, std::string *reply_oob_data,
				const char *match = nullptr, std::vector<std::string> *string_value = nullptr, std::vector<int> *int_value = nullptr) const;
		int read_sector(unsigned int sector_size, unsigned int sector, std::string &data) const;
		int write_sector(unsigned int sector, const std::string &data,
				unsigned int &written, unsigned int &erased, unsigned int &skipped, bool simulate) const;
		void get_checksum(unsigned int sector, unsigned int sectors,
				std::string &checksum) const;

	private:

		GenericSocket &channel;
		bool verbose, debug, raw, provide_checksum, request_checksum;
		unsigned int broadcast_group_mask;
};
#endif
