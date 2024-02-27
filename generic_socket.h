#ifndef _generic_socket_h_
#define _generic_socket_h_

#include "espifconfig.h"

#include <netinet/in.h>
#include <string>

class GenericSocket
{
	friend class Espif;
	friend class Util;

	protected:

		GenericSocket(const EspifConfig &);
		~GenericSocket() noexcept;

		bool send(std::string &data, int timeout = 500) const;
		bool receive(std::string &data, int timeout = 500, struct sockaddr_in *remote_host = nullptr) const;
		void drain(int timeout = 500) const noexcept;
		void connect();
		void disconnect() noexcept;

	private:

		int socket_fd;
		struct sockaddr_in saddr;

		const EspifConfig config;
};
#endif
