#!/usr/bin/perl -w

use strict;
use warnings;
use Esp::IF;
use Getopt::Long;
use Try::Tiny;

my($option_host);
my($option_port) = "24";
my($option_flash_sector_size) = 4096;
my($option_broadcast) = 0;
my($option_multicast) = 0;
my($option_raw) = 0;
my($option_verbose) = 0;
my($option_debug) = 0;
my($option_no_provide_checksum) = 0;
my($option_no_request_checksum) = 0;
my($option_use_tcp) = 0;
my($option_dontwait) = 0;
my($option_broadcast_group_mask) = 0;
my($option_multicast_burst) = 1;
my($capture);

GetOptions(
			"host=s" =>					\$option_host,
			"port=i" =>					\$option_port,
			"flash-sector-size=i" =>	\$option_flash_sector_size,
			"broadcast" =>				\$option_broadcast,
			"multicast" =>				\$option_multicast,
			"raw" =>					\$option_raw,
			"verbose" =>				\$option_verbose,
			"debug" =>					\$option_debug,
			"no-provide-checksum" =>	\$option_no_provide_checksum,
			"no-request-checksum" =>	\$option_no_request_checksum,
			"use-tcp" =>				\$option_use_tcp,
			"dontwait" =>				\$option_dontwait,
			"broadcast-group=i" =>		\$option_broadcast_group_mask,
			"multicast-burst" =>		\$option_multicast_burst);

if(!defined($option_host))
{
	die("host required") if(!defined($option_host = shift(@ARGV)));
}

try
{
	my($espifconfig) = new Esp::IF::EspifConfig;

	$$espifconfig{"host"} = $option_host;
	$$espifconfig{"command_port"} = $option_port;
	$$espifconfig{"use_tcp"} = $option_use_tcp;
	$$espifconfig{"broadcast"} = $option_broadcast;
	$$espifconfig{"multicast"} = $option_multicast;
	$$espifconfig{"raw"} = $option_raw;
	$$espifconfig{"debug"} = $option_debug;
	$$espifconfig{"verbose"} = $option_verbose;
	$$espifconfig{"provide_checksum"} = !$option_no_provide_checksum;
	$$espifconfig{"request_checksum"} = !$option_no_request_checksum;
	$$espifconfig{"dontwait"} = $option_dontwait;
	$$espifconfig{"broadcast_group_mask"} = $option_broadcast_group_mask;
	$$espifconfig{"multicast_burst"} = $option_multicast_burst;

	my($espif) = new Esp::IF::Espif($espifconfig);

	if($option_broadcast || $option_multicast)
	{
		$capture = $espif->multicast(join(" ", @ARGV));
	}
	else
	{
		$capture = $espif->send(join(" ", @ARGV));
	}
}
catch
{
	$capture = sprintf("FAILED: %s", $_);
};

printf("%s", $capture);
