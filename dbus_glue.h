#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <dbus/dbus.h>

#include <string>

class dBusGlue
{
	public:

		dBusGlue() = delete;
		dBusGlue(const dBusGlue &) = delete;

		dBusGlue(std::string bus);
		bool get_message(int *type = nullptr, std::string *interface = nullptr, std::string *method = nullptr);
		bool receive_string(std::string &, std::string *error_message);
		bool receive_uint32_uint32_string_string(uint32_t &, uint32_t &, std::string &, std::string &, std::string *error_message);
		bool send_string(std::string reply_string);
		bool send_uint64_uint32_uint32_string_double(uint64_t, uint32_t, uint32_t, std::string, double);
		std::string inform_error(std::string reason);
		void reset();

	private:

		DBusError dbus_error;
		DBusConnection *bus_connection;
		DBusMessage *incoming_message;
};
