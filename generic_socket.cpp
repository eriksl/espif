#include "generic_socket.h"
#include "util.h"
#include "exception.h"

#include <string>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <iostream>

GenericSocket::GenericSocket(const std::string &host_in, const std::string &service_in, unsigned int buffer_size_in,
		bool tcp_in, bool broadcast_in, bool multicast_in, bool verbose_in)
	: socket_fd(-1), service(service_in), buffer_size(buffer_size_in),
		tcp(tcp_in), broadcast(broadcast_in), multicast(multicast_in), verbose(verbose_in)
{
	memset(&saddr, 0, sizeof(saddr));

	if(multicast)
		host = std::string("239.255.255.") + host_in;
	else
		host = host_in;

	if(multicast || broadcast)
		tcp = false;

	this->connect();
}

GenericSocket::~GenericSocket() noexcept
{
	this->disconnect();
}

void GenericSocket::connect()
{
	struct addrinfo hints;
	struct addrinfo *res = nullptr;
	int socket_argument;

	if(tcp)
		socket_argument = SOCK_STREAM | SOCK_NONBLOCK;
	else
		socket_argument = SOCK_DGRAM;

	if((socket_fd = socket(AF_INET, socket_argument, 0)) < 0)
		throw(hard_exception("socket failed"));

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = tcp ? SOCK_STREAM : SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICSERV;

	if(getaddrinfo(host.c_str(), service.c_str(), &hints, &res))
	{
		if(res)
			freeaddrinfo(res);
		throw(hard_exception("unknown host"));
	}

	if(!res || !res->ai_addr)
		throw(hard_exception("unknown host"));

	saddr = *(struct sockaddr_in *)res->ai_addr;
	freeaddrinfo(res);

	if(broadcast)
	{
		int arg = 1;

		if(setsockopt(socket_fd, SOL_SOCKET, SO_BROADCAST, &arg, sizeof(arg)))
		{
			if(verbose)
				perror("setsockopt SO_BROADCAST\n");
			throw(hard_exception("set broadcast"));
		}
	}

	if(multicast)
	{
		struct ip_mreq mreq;
		int arg = 3;

		if(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &arg, sizeof(arg)))
			throw(hard_exception("multicast: cannot set mc ttl"));

		arg = 0;

		if(setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &arg, sizeof(arg)))
			throw(hard_exception("multicast: cannot set loopback"));

		mreq.imr_multiaddr = saddr.sin_addr;
		mreq.imr_interface.s_addr = INADDR_ANY;

		if(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)))
			throw(hard_exception("multicast: cannot join mc group"));
	}

	if(tcp)
	{
		struct pollfd pfd;

		pfd.fd = socket_fd;
		pfd.events = POLLOUT;
		pfd.revents = 0;

		try
		{
			if((::connect(socket_fd, (const struct sockaddr *)&saddr, sizeof(saddr))) && (errno != EINPROGRESS))
				throw("tcp connect: connect failed");

			if(poll(&pfd, 1, 500) != 1)
				throw("tcp connect: timeout");

			if(pfd.revents & (POLLERR | POLLHUP))
				throw("tcp connect: connect event error");

			if(!(pfd.revents & POLLOUT))
				throw("tcp connect: connect event unfinished");
		}
		catch(const char *e)
		{
			throw(hard_exception(host + ": " + e));
		}
	}
}

void GenericSocket::disconnect() noexcept
{
	if(socket_fd >= 0)
		close(socket_fd);

	socket_fd = -1;
}

bool GenericSocket::send(std::string &data, int timeout) const
{
	struct pollfd pfd;
	int length;

	pfd.fd = socket_fd;
	pfd.events = POLLOUT | POLLERR | POLLHUP;
	pfd.revents = 0;

	if(data.length() == 0)
	{
		if(verbose)
			std::cout << "send: empty buffer" << std::endl;
		return(true);
	}

	if(poll(&pfd, 1, timeout) != 1)
	{
		if(verbose)
			std::cout << "send: timeout" << std::endl;
		return(false);
	}

	if(pfd.revents & (POLLERR | POLLHUP))
	{
		if(verbose)
			std::cout << "send: socket error" << std::endl;
		return(false);
	}

	if(tcp)
	{
		if((length = ::send(socket_fd, data.data(), data.length(), 0)) <= 0)
			return(false);
	}
	else
	{
		if((length = ::sendto(socket_fd, data.data(), data.length(), 0, (const struct sockaddr *)&this->saddr, sizeof(this->saddr))) <= 0)
			return(false);
	}

	data.erase(0, length);

	return(true);
}

bool GenericSocket::receive(std::string &data, int timeout, struct sockaddr_in *remote_host) const
{
	int length;
	char buffer[2 * buffer_size];
	socklen_t remote_host_length = sizeof(*remote_host);
	struct pollfd pfd = { .fd = socket_fd, .events = POLLIN | POLLERR | POLLHUP, .revents = 0 };

	if(poll(&pfd, 1, timeout) != 1)
	{
		if(verbose)
			std::cout << std::endl << boost::format("receive: timeout, length: %u") << data.length() << std::endl;
		return(false);
	}

	if(pfd.revents & POLLERR)
	{
		if(verbose)
			std::cout << std::endl << "receive: POLLERR" << std::endl;
		return(false);
	}

	if(pfd.revents & POLLHUP)
	{
		if(verbose)
			std::cout << std::endl << "receive: POLLHUP" << std::endl;
		return(false);
	}

	if(tcp)
	{
		if((length = ::recv(socket_fd, buffer, sizeof(buffer) - 1, 0)) <= 0)
		{
			if(verbose)
				std::cout << std::endl << "tcp receive: length <= 0" << std::endl;
			return(false);
		}
	}
	else
	{
		if((length = ::recvfrom(socket_fd, buffer, sizeof(buffer) - 1, 0, (sockaddr *)remote_host, &remote_host_length)) <= 0)
		{
			if(verbose)
				std::cout << std::endl << "udp receive: length <= 0" << std::endl;
			return(false);
		}
	}

	data.append(buffer, (size_t)length);

	return(true);
}

void GenericSocket::drain(int timeout) const noexcept
{
	struct pollfd pfd;
	enum { drain_packets_buffer_size = 4, drain_packets = 16 };
	char *buffer = (char *)alloca(buffer_size * drain_packets_buffer_size);
	int length;
	int bytes = 0;
	int packet = 0;

	if(verbose)
		std::cout << boost::format("draining %u...") % timeout << std::endl;

	for(packet = 0; packet < drain_packets; packet++)
	{
		pfd.fd = socket_fd;
		pfd.events = POLLIN | POLLERR | POLLHUP;
		pfd.revents = 0;

		if(poll(&pfd, 1, timeout) != 1)
			break;

		if(pfd.revents & (POLLERR | POLLHUP))
			break;

		if(tcp)
		{
			if((length = ::recv(socket_fd, buffer, buffer_size * drain_packets_buffer_size, 0)) < 0)
				break;
		}
		else
		{
			if((length = ::recvfrom(socket_fd, buffer, buffer_size * drain_packets_buffer_size, 0, (struct sockaddr *)0, 0)) < 0)
				break;
		}

		if(verbose)
			std::cout << Util::dumper("drain", std::string(buffer, length)) << std::endl;

		bytes += length;
	}

	if(verbose && (packet > 0))
		std::cout << boost::format("drained %u bytes in %u packets") % bytes % packet << std::endl;
}
