#ifndef _exception_h_
#define _exception_h_

#include <exception>
#include <string>
#include <boost/format.hpp>

class espif_exception : public std::exception
{
	public:

		espif_exception() = delete;
		espif_exception(const std::string &what);
		espif_exception(const char *what);
		espif_exception(const boost::format &what);

		const char *what() const noexcept;

	private:

		const std::string what_string;
};

class hard_exception : public espif_exception
{
	public:

		hard_exception() = delete;
		hard_exception(const std::string &what);
		hard_exception(const char *what);
		hard_exception(const boost::format &what);
};

class transient_exception : public espif_exception
{
	public:

		transient_exception() = delete;
		transient_exception(const std::string &what);
		transient_exception(const char *what);
		transient_exception(const boost::format &what);
};

#endif
