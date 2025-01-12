%module "Esp::IF"

%include <std_string.i>
%include <exception.i>
%include <std_except.i>

%{
#include <iostream>
#include "espif.h"
#include "espifconfig.h"
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

%include "espif.h"
%include "espifconfig.h"

%perlcode %{
	sub new_EspifConfig($)
	{
		my($config) = @_;
		my($key, $value, $espifconfig);

		$espifconfig = new Esp::IF::EspifConfig;

		foreach $key (keys(%$config))
		{
			$value = $$config{$key};
			$espifconfig->{$key} = $value;
		}

		return($espifconfig);
	}
%}
