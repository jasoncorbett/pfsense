<?php
/****h* pfSense/config
 * NAME
 *   config.inc - Functions to manipulate config.xml
 * DESCRIPTION
 *   This include contains various config.xml specific functions.
 * HISTORY
 * $Id$
 ******

	config.inc
	Copyright (C) 2004-2010 Scott Ullrich
	All rights reserved.

	originally part of m0n0wall (http://m0n0.ch/wall)
	Copyright (C) 2003-2004 Manuel Kasper <mk@neon1.net>.
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.


	pfSense_BUILDER_BINARIES:	/sbin/mount	/sbin/umount	/sbin/halt	/sbin/fsck
	pfSense_MODULE:	config
*/

if (!function_exists('platform_booting'))
	require_once('globals.inc');

/* do not load this file twice. */
//if (in_array("/etc/inc/config.inc", get_included_files()))
//	return;

// Set the memory limit to 128M on i386.  When someone has something like 500+ tunnels
// the parser needs quite a bit of ram.   Do not remove this line unless you
// know what you are doing.  If in doubt, check with dev@ _/FIRST/_!
if(!$ARCH)
	$ARCH = php_uname("m");

// Set memory limit to 256M on amd64.
if($ARCH == "amd64")
	ini_set("memory_limit","256M");
else
	ini_set("memory_limit","128M");

/* include globals from notices.inc /utility/XML parser files */
require_once("notices.inc");
require_once("util.inc");
require_once("IPv6.inc");
require_once('config.lib.inc');
if(file_exists("/cf/conf/use_xmlreader"))
	require_once("xmlreader.inc");
else
	require_once("xmlparse.inc");
require_once("crypt.inc");

/* read platform */
if (file_exists("{$g['etc_path']}/platform")) {
	$g['platform'] = chop(file_get_contents("{$g['etc_path']}/platform"));
} else {
	$g['platform'] = "unknown";
}

/* if /debugging exists, lets set $debugging
   so we can output more information */
if(file_exists("/debugging")) {
	$debugging = true;
	$g['debug'] = true;
}

if(platform_booting(true)) {
	echo ".";
	if(file_exists("/cf/conf/config.xml")) {
		$config_contents = file_get_contents("/cf/conf/config.xml");
		if(stristr($config_contents, "<m0n0wall>") == true) {
			echo ".";
			/* user has just upgraded to m0n0wall, replace root xml tags */
			log_error(gettext("Upgrading m0n0wall configuration to pfSense... "));
			$config_contents = str_replace("m0n0wall","pfsense", $config_contents);
			if (!config_validate("{$g['conf_path']}/config.xml"))
				log_error(gettext("ERROR!  Could not convert m0n0wall -> pfsense in config.xml"));
			conf_mount_rw();
			file_put_contents("/cf/conf/config.xml", $config_contents);
			conf_mount_ro();
		}
		unset($config_contents);
	}
	/* if our config file exists bail out, we're already set. */
	else if (!file_exists($g['cf_conf_path'] . "/config.xml")  ) {
		echo ".";
		/* find the device where config.xml resides and write out an fstab */
		unset($cfgdevice);
		echo ".";
		/* check if there's already an fstab (NFS booting?) */
		if (!file_exists("{$g['etc_path']}/fstab")) {
			echo ".";
			if (strstr($g['platform'], "cdrom")) {
				/* config is on floppy disk for CD-ROM version */
				$cfgdevice = $cfgpartition = "fd0";
				$_gb = exec('/sbin/dmesg -a', $dmesg);
				if(preg_match("/da0/", $dmesg) == true) {
					$cfgdevice = $cfgpartition = "da0" ;
					if (mwexec("/sbin/mount -r /dev/{$cfgdevice} /cf")) {
						/* could not mount, fallback to floppy */
						$cfgdevice = $cfgpartition = "fd0";
					}
				}
				unset($dmesg);
				$cfgfstype = "msdosfs";
				echo gettext("CDROM build") . "\n";
				echo "   " . gettext("CFG:") . " {$cfgpartition}\n";
				echo "   " . gettext("CFG:") . " {$cfgpartition}\n";
				echo "  " . gettext("TYPE:") . " {$cfgfstype}\n";
			} else {
				echo ".";
				/* probe kernel known disks until we find one with config.xml */
				$disks = explode(" ", get_single_sysctl("kern.disks"));
				foreach ($disks as $mountdisk) {
					/* skip mfs mounted filesystems */
					if (strstr($mountdisk, "md"))
						continue;
					if (mwexec("/sbin/mount -r /dev/{$mountdisk}a {$g['cf_path']}") == 0) {
						if (file_exists("{$g['cf_conf_path']}/config.xml")) {
							/* found it */
							$cfgdevice = $mountdisk;
							$cfgpartition = $cfgdevice . "a";
							$cfgfstype = "ufs";
							printf(gettext("Found configuration on %s.%s"), $cfgdevice, "\n");
						}

						mwexec("/sbin/umount -f {$g['cf_path']}");

						if ($cfgdevice)
							break;
					}
					if (mwexec("/sbin/mount -r /dev/{$mountdisk}d {$g['cf_path']}") == 0) {
						if(platform_booting()) echo ".";
						if (file_exists("{$g['cf_conf_path']}/config.xml")) {
							/* found it */
							$cfgdevice = $mountdisk;
							$cfgpartition = $cfgdevice . "d";
							$cfgfstype = "ufs";
							printf(gettext("Found configuration on %s.%s"), $cfgdevice, "\n");
						}

						mwexec("/sbin/umount -f {$g['cf_path']}");

						if ($cfgdevice)
							break;
					}
				}
			}
			echo ".";
			if (!$cfgdevice) {
				$last_backup = discover_last_backup();
				if($last_backup) {
					log_error(gettext("No config.xml found, attempting last known config restore."));
					file_notice("config.xml", gettext("No config.xml found, attempting last known config restore."), "pfSenseConfigurator", "");
					restore_backup("/cf/conf/backup/{$last_backup}");
				} else {
					log_error(gettext("No config.xml or config backups found, resetting to factory defaults."));
					restore_backup('/conf.default/config.xml');
				}
			}

			/* write device name to a file for rc.firmware */
			file_put_contents("{$g['varetc_path']}/cfdevice", $cfgdevice . "\n");

			/* write out an fstab */

			$fstab = "/dev/{$cfgpartition} {$g['cf_path']} {$cfgfstype} ro,noatime 1 1\n";
			$fstab .= "proc /proc procfs rw 0 0\n";
			file_put_contents("{$g['etc_path']}/fstab", $fstab);
		}
		echo ".";
		/* mount all filesystems */
		mwexec("/sbin/mount -a");
	}
	echo ".";
}

$config = parse_config();

/* set timezone */
$timezone = $config['system']['timezone'];
if (!$timezone)
	$timezone = "Etc/UTC";
date_default_timezone_set("$timezone");

/* compatibility with older configs using vlan "tags" */	
if (($config['version'] < 11) && (is_array($config['vlans']['vlan']) && count($config['vlans']['vlan'])) {
	foreach ($config['vlans']['vlan'] as &$vlan) {
		if (empty($vlan['vid']) && !empty($vlan['tag'])) {
			$vlan['vid']  =  $vlan['tag'];
			unset($vlan['tag']);
		}
	}

if($config_parsed == true) {
	/* process packager manager custom rules */
	if(is_dir("/usr/local/pkg/parse_config")) {
		run_plugins("/usr/local/pkg/parse_config/");
	}
}

?>
