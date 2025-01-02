#include "dbus_glue.h"
#include "exception.h"

#include <stdint.h>
#include <stdbool.h>
#include <dbus/dbus.h>

#include <string>
#include <iostream>
#include <boost/format.hpp>

dBusGlue::dBusGlue(std::string bus) : bus_connection(nullptr), incoming_message(nullptr)
{
	dbus_error_init(&dbus_error);
	int rv;

	bus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);

	if(dbus_error_is_set(&dbus_error))
		throw(hard_exception(std::string("dbus bus get failed: ") + dbus_error.message));

	if(!bus_connection)
		throw(hard_exception("dbus bus get failed (bus_connection = nullptr)"));

	rv = dbus_bus_request_name(bus_connection, bus.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbus_error);

	if(dbus_error_is_set(&dbus_error))
		throw(hard_exception(std::string("dbus request name failed: ") + dbus_error.message));

	if(rv != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		throw(hard_exception("dbus request name: not primary owner: "));
}

bool dBusGlue::get_message(int *type, std::string *interface, std::string *method)
{
	if(!dbus_connection_read_write_dispatch(bus_connection, -1))
		return(false);

	if(!(incoming_message = dbus_connection_pop_message(bus_connection)))
		return(false);

	if(type)
		*type = dbus_message_get_type(incoming_message);

	if(interface)
		*interface = dbus_message_get_interface(incoming_message);

	if(method)
		*method = dbus_message_get_member(incoming_message);

	return(true);
}

bool dBusGlue::receive_string(std::string &p1, std::string *error_message)
{
	const char *s1;

	dbus_message_get_args(incoming_message, &dbus_error, DBUS_TYPE_STRING, &s1, DBUS_TYPE_INVALID);

	if(dbus_error_is_set(&dbus_error))
	{
		*error_message = dbus_error.message;
		dbus_error_free(&dbus_error);
		return(false);
	}

	p1 = s1;

	*error_message = "";
	return(true);
}

bool dBusGlue::receive_uint32_uint32_string_string(uint32_t &p1, uint32_t &p2, std::string &p3, std::string &p4, std::string *error_message)
{
	dbus_uint32_t s1, s2;
	const char *s3, *s4;

	dbus_message_get_args(incoming_message, &dbus_error, DBUS_TYPE_UINT32, &s1, DBUS_TYPE_UINT32, &s2, DBUS_TYPE_STRING, &s3, DBUS_TYPE_STRING, &s4, DBUS_TYPE_INVALID);

	if(dbus_error_is_set(&dbus_error))
	{
		*error_message = dbus_error.message;
		dbus_error_free(&dbus_error);
		return(false);
	}

	p1 = s1;
	p2 = s2;
	p3 = s3;
	p4 = s4;

	*error_message = "";
	return(true);
}

bool dBusGlue::send_string(std::string reply_string)
{
	DBusMessage *reply_message;
	const char *reply_cstr;

	reply_cstr = reply_string.c_str();

	if(!(reply_message = dbus_message_new_method_return(incoming_message)))
		return(false);

	if(!dbus_message_append_args(reply_message, DBUS_TYPE_STRING, &reply_cstr, DBUS_TYPE_INVALID))
	{
		dbus_message_unref(reply_message);
		return(false);
	}

	if(!dbus_connection_send(bus_connection, reply_message, NULL))
		return(false);

	dbus_message_unref(reply_message);

	return(true);
}

bool dBusGlue::send_uint64_uint32_uint32_string_double(uint64_t p1, uint32_t p2, uint32_t p3, std::string p4, double p5)
{
	DBusMessage *reply_message;
	const char *cstr;

	cstr = p4.c_str();

	if(!(reply_message = dbus_message_new_method_return(incoming_message)))
		return(false);

	if(!dbus_message_append_args(reply_message, DBUS_TYPE_UINT64, &p1, DBUS_TYPE_UINT32, &p2, DBUS_TYPE_UINT32, &p3, DBUS_TYPE_STRING, &cstr, DBUS_TYPE_DOUBLE, &p5, DBUS_TYPE_INVALID))
	{
		dbus_message_unref(reply_message);
		return(false);
	}

	if(!dbus_connection_send(bus_connection, reply_message, NULL))
	{
		dbus_message_unref(reply_message);
		return(false);
	}

	dbus_message_unref(reply_message);

	return(true);
}

std::string dBusGlue::inform_error(std::string reason)
{
	DBusMessage *error_message;

	if(!(error_message = dbus_message_new_error(incoming_message, DBUS_ERROR_FAILED, reason.c_str())))
		throw(hard_exception("method error - error in dbus_message_new_error"));

	if(!dbus_connection_send(bus_connection, error_message, NULL))
		throw(hard_exception("method error - error in dbus_connection_send"));

	dbus_message_unref(error_message);

	return(reason);
}

void dBusGlue::reset()
{
	if(bus_connection)
		dbus_connection_flush(bus_connection);

	if(incoming_message)
	{
		dbus_message_unref(incoming_message);
		incoming_message = nullptr;
	}
}
