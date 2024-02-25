#!/usr/bin/perl -w

use strict;
use warnings;
use lib qw(. ./Esp);
use Esp::IF;
use Getopt::Long;

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

my($channel) = new Esp::IF::GenericSocket($option_host, $option_port, $option_flash_sector_size, $option_use_tcp, $option_broadcast, $option_multicast, $option_verbose);
my($util) = new Esp::IF::Util($channel, $option_verbose, $option_debug, $option_raw, !$option_no_provide_checksum, $option_no_request_checksum, $option_broadcast_group_mask);
my($command) = new Esp::IF::Command($util, $channel, $option_raw, $option_no_provide_checksum, $option_no_request_checksum, $option_dontwait, $option_debug,
		$option_verbose, $option_broadcast_group_mask, $option_multicast_burst, $option_flash_sector_size);
my($capture);

if($option_broadcast || $option_multicast)
{
	$capture = $command->multicast(join(" ", @ARGV));
}
else
{
	$capture = $command->send(join(" ", @ARGV));
}

printf("%s", $capture);
