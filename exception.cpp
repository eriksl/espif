#include "exception.h"

espif_exception::espif_exception(const std::string &what)
		:
	what_string(what)
{
}

espif_exception::espif_exception(const char *what)
		:
	what_string(what)
{
}

espif_exception::espif_exception(const boost::format &what)
		:
	what_string(what.str())
{
}

const char *espif_exception::what() const noexcept
{
	return(what_string.c_str());
}

hard_exception::hard_exception(const std::string &what)
		:
	espif_exception(what)
{
}

hard_exception::hard_exception(const char *what)
		:
	espif_exception(what)
{
}

hard_exception::hard_exception(const boost::format &what)
		:
	espif_exception(what)
{
}

transient_exception::transient_exception(const std::string &what)
		:
	espif_exception(what)
{
}

transient_exception::transient_exception(const char *what)
		:
	espif_exception(what)
{
}

transient_exception::transient_exception(const boost::format &what)
		:
	espif_exception(what)
{
}
