%module "Esp::IF"
%include "std_string.i"

%{
#include <iostream>
#include "command.h"
#include "generic_socket.h"
#include "util.h"
#include "espif.h"
#include "exception.h"
%}

%include "exception.h"

%exception
{
	try
	{
		$action
	}
	catch(const hard_exception &e)
	{
		std::cerr << "Esp::IF: hard exception: " << e.what() << std::endl;
		die("abort");
	}
	catch(const transient_exception &e)
	{
		std::cerr << "Esp::IF: transient exception: " << e.what() << std::endl;
		die("abort");
	}
	catch(const espif_exception &e)
	{
		std::cerr << "Esp::IF: unspecified exception: " << e.what() << std::endl;
		die("abort");
	}
	catch(const std::exception &e)
	{
		std::cerr << "Esp::IF: STD exception: " << e.what() << std::endl;
		die("abort");
	}
	catch(const std::string &e)
	{
		std::cerr << "Esp::IF: string exception: " << e << std::endl;
		die("abort");
	}
	catch(const char *e)
	{
		std::cerr << "Esp::IF: charp exception: " << e << std::endl;
		die("abort");
	}
	catch(...)
	{
		die("Esp::IF: generic exception\nabort");
	}
}

%include "command.h"
%include "generic_socket.h"
%include "util.h"
%include "espif.h"
