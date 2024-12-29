#!/usr/bin/perl -w

use lib qw(/data/src/intern/espif /data/src/intern/espif/Esp);

use strict;
use warnings;
use utf8;

use feature "signatures";

use Data::Dumper;
use Esp::IF;
use Try::Tiny;

use open qw(:std :encoding(UTF-8));

$Data::Dumper::Sortkeys = 1;
STDERR->autoflush(1);

my(%broadcast_groups) =	(
		"all" => 0,
		"text display" => 1,
		"graphic display" => 2,
		"esp2x" => 3,
		"esp3x" => 4,
		"esp4x" => 5,
		"esp5x" => 6,
);

sub get_esp($host, $args)
{
	try
	{
		my($espifconfig) = Esp::IF::new_EspifConfig({"host" => $host, "broadcast" => 1, "transport" => "udp", "broadcast_group_mask" => 1, "multicast_burst" => 5});
		my($espif) = new Esp::IF::Espif($espifconfig);
		return($espif->send($args));
	}
	catch
	{
		printf STDERR ("E get_esp(%s %s)\n", $host, $args);
		return(undef);
	};
}

sub esp_send_cmd($destination, $string = undef)
{
	my($broadcast_group, $cmd, $capture);

	return if(!defined($string) or ($string eq ""));

	if(exists($broadcast_groups{$destination}))
	{
		$broadcast_group = 1 << $broadcast_groups{$destination};
		printf("* esp_send_cmd_broadcast [%d -> %s] = %s\n", $broadcast_group, $destination, $string);

		try
		{
			my($espifconfig) = Esp::IF::new_EspifConfig({"host" => "172.19.128.255", "broadcast" => 1, "transport" => "udp", "broadcast_group_mask" => $broadcast_group, "multicast_burst" => 5});
			my($espif) = new Esp::IF::Espif($espifconfig);
			$capture = $espif->multicast($string);
		}
		catch
		{
			printf STDERR ("E esp_send_cmd_broadcast %s: %s\n", $destination, $capture);
		};
	}
	else
	{
		printf("* esp_send_cmd [%s] = %s\n", $destination, $string);

		try
		{
			my($espifconfig) = Esp::IF::new_EspifConfig({"host" => $destination, "transport" => "tcp"});
			my($espif) = new Esp::IF::Espif($espifconfig);
			$capture = $espif->send($string);
		}
		catch
		{
			printf STDERR ("E esp_send_cmd %s\n", $destination);
		};
	}

	printf("* %s\n", $capture);
}

esp_send_cmd("all", "id");

sub send_message($destination, $slot, $text)
{
	printf("* send_message %s [%s] = \"%s\"\n", $destination , $slot, $text);
	esp_send_cmd($destination, "ds $slot 300 $text");
}
