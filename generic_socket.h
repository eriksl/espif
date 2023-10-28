#ifndef _generic_socket_h_
#define _generic_socket_h_
#include <netinet/in.h>
#include <string>

class GenericSocket
{
	private:

		int socket_fd;
		std::string host;
		const std::string service;
		struct sockaddr_in saddr;
		const unsigned int buffer_size;
		bool tcp;
		const bool broadcast, multicast, verbose;

	public:
		GenericSocket(const std::string &host, const std::string &port, unsigned int buffer_size, bool tcp, bool broadcast, bool multicast, bool verbose);
		~GenericSocket() noexcept;

		bool send(std::string &data, int timeout = 500) const;
		bool receive(std::string &data, int timeout = 500, struct sockaddr_in *remote_host = nullptr) const;
		void drain(int timeout = 500) const noexcept;
		void connect();
		void disconnect() noexcept;
};
#endif
