#!/bin/bash
#
# QLogic ISP2xxx device driver debug level tuning tool.
# Copyright (C) 2012 QLogic Corporation
# Copyright (C) 2018 - 2020 Marvell.
# (www.qlogic.com)
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# qdiag.sh  : QLogic diagnostic script to capture relevant debug info.
#
  version="1.06"
#
# NOTES: To add additional commands/files, please goto the end of the file
#

trap 'script_done' 0 2 15

# -- not used now, this will make all files unique --
## All commands will be prefixed by CMD_START_N
#CMD_START_N=1
## All files will be prefixed by FILE_START_N
#FILE_START_N=2

OIFS="$IFS"

function usage()
{
    cat <<- 'EOF' >&2
Usage: qdiag.sh [-#] [-r] [-s] [-v] [-d <dir>]
    # : Number from 0-5, indicating verbosity required (default 0)
    d : To specify an alternate directory for result (default /var)
    r : Capture only runtime data
    s : Capture only sysfs attributes
    v : verbosity, displays what script is capturing (default: off)

    NOTE: default options (-0rs). Remove -s for faster action.
EOF
    exit 2
}

usr_def_opt="0rs"; usr_opt=""; opt_v=0

function read_opts()
{
    # All options to control command o/p goto "usr_opt". Script specific
    # options go separate
    DIR_NAME="qdiag"
    QDIAG_FILE_BASE="/var"; TMP_BASE="/var/$DIR_NAME"
    local o i

    while getopts 012345rsvd: o; do
	case "$o" in
	#a)  arg="$OPTARG";;
	v)  opt_v=1 ;;
	d)  opt_d="$OPTARG"
	    if [[ ! -d "$opt_d" ]];then
		echo "ERROR: <$opt_d>: Invalid directory" >&2
		exit 2;
	    fi
	    opt_d=$(\cd $opt_d;pwd) # To take care of relative paths.
	    QDIAG_FILE_BASE="$opt_d"; TMP_BASE="$opt_d/$DIR_NAME"
	;;
	[rs])
	    usr_opt=${usr_opt}$o ;;
	[0-5])
	    usr_opt=${usr_opt}$o
	    # Put all numbers upto n in usr_opt
	    i=-1; while((++i<=o)); do usr_opt="${usr_opt}$i"; done
	    usr_opt="${usr_opt}d"   # Capture dyn info too
	    ;;
	*)  usage ;;
	esac
    done
    [[ "$usr_opt" = "" ]] && {
	[[ $opt_v -eq 1 ]]  && echo "adding default options: $usr_def_opt.."
	usr_opt=$usr_def_opt;
    }
    shift $((OPTIND-1))

    # -- env variable init --
    YR=$(date +%Y); MD=$(date +%m%d);
    H=$(date +%H); M=$(date +%M); S=$(date +%S)
    TM="$H:$M:$S" TMF="$H$M$S"

    QDIAG_FILE="$QDIAG_FILE_BASE/${DIR_NAME}_${YR}_${MD}_${TMF}.tar.gz"

    TMP_DIR="$TMP_BASE/$YR/$MD/$TM"

    CMD_FILE="$TMP_DIR/z-commands"
    FILES_FILE="$TMP_DIR/z-files"
    META_FILE="$TMP_DIR/z-meta.txt"
    CMD_OUT_DIR="$TMP_DIR"
    FILE_OUT_DIR="$TMP_DIR"

    me_created=0;

}

function script_init()
{
    tmp_dir_base="${TMP_BASE%/*}"
    if [[ ! -w "$tmp_dir_base" ]];then
	echo "ERROR: Cannot write to $tmp_dir_base, try -d option" >&2
	exit 2;
    fi

    if [[ -d $TMP_BASE ]];then
	echo "Temporary directory $TMP_BASE exists. Looks like the script" >&2
	echo "either is running or crashed earlier." >&2
	echo -e "\nAborting execution!!" >&2
	exit 2;
    fi
    mkdir -p $TMP_DIR
    me_created=1
}

function read_commands()
{
    local i cmd opt line
    sed -n '/^--- Commands ---$/,/^$/p' $0 |grep -v '^#' > $CMD_FILE

    let i=0
    while read line; do
	[[ "$line" = '--- Commands ---' || "$line" = "" ]] && continue

	cmd="${line#+}"; opt=""
	if [[ "${cmd# }" = "$cmd" ]];then   # Options supplied
	    opt=${cmd%% *}
	    cmd=${cmd#$opt }
	fi
	cmd="$(echo $cmd |sed -e 's/^[ ]*//')" # removing leading space

	COMMAND_OPTS[i]="$opt"
	COMMANDS[i]="$cmd"
	let i=i+1
    done < $CMD_FILE
    let NUM_CMDS=i
}

function read_files()
{
    local i line file opt

    sed -n '/^--- Files ---$/,/^$/p' $0 |grep -v '^#' > $FILES_FILE

    let i=0
    while read line; do
	[[ "$line" = '--- Files ---' || "$line" = "" ]] && continue

	file=${line#+}; opt=""
	if [[ "${file# }" = "$file" ]];then   # Options supplied
	    opt=${file%% *}
	    file=${file#$opt}
	fi
	file="$(echo $file |sed -e 's/^[ ]*//')" # removing leading space

	FILE_OPTS[i]="$opt"
	FILES[i]="$file"
	let i=i+1
    done < $FILES_FILE
    let NUM_FILES=i
}

function compute_prefix()
{
    local n

    n=$((NUM_FILES > NUM_CMDS ? NUM_FILES : NUM_CMDS))
    CMD_START_N=$((CMD_START_N * 10 ** ${#n} ))
    FILE_START_N=$((FILE_START_N * 10 ** ${#n} ))
}

function make_fname()
{
    local fname
    fname="$*"
    fname=$(echo "$fname" |tr / '.')      # All /   => .
    fname=$(echo "$fname" |tr ' ' '_')    # All " " => _
    fname=$(echo "$fname" |tr -d '-')     # Remove all -
    echo -n "$fname"
}

function option_match()
{
    local cmd_opt t

    cmd_opt=$1
    [[ "$cmd_opt" = "" ]] && cmd_opt="0"
    t=$(echo "$usr_opt" |tr -d "[$cmd_opt]")
    [[ "$t" != "$usr_opt" ]] && return 0
    return 1
}

function run_commands()
{
    local i cmd opt fname

    [[ ! -d "$CMD_OUT_DIR" ]] && mkdir $CMD_OUT_DIR
    cd $CMD_OUT_DIR
    let i=0 num_done=0
    [[ $opt_v -eq 1 ]]  && echo -n "collecting command outputs.."
    while((i<NUM_CMDS)); do
	#printf "%u ==> <%s>\n" $i "${COMMANDS[$i]}"
	cmd="${COMMANDS[$i]}"
	opt="${COMMAND_OPTS[$i]}"

	option_match $opt || { let i=i+1; continue; }
	let num_done=num_done+1

	fname=$(make_fname "$cmd")
	#fname=$(($CMD_START_N+$i))-$fname
	fname="c-"$fname

	#echo "cmd => $cmd"; let i=i+1; continue;

	{
	    echo "# $cmd"
	    eval $cmd
	} > $fname 2>&1 &
	let i=i+1
    done
    wait
    [[ $opt_v -eq 1 ]] && echo "done ($num_done)"
    cd $OLDPWD
}

function get_files()
{
    local i file opt fname

    [[ ! -d "$FILE_OUT_DIR" ]] && mkdir $FILE_OUT_DIR

    cd $FILE_OUT_DIR
    let i=0 num_done=0
    [[ $opt_v -eq 1 ]] && echo -n "collecting files.."
    while((i<NUM_FILES)); do
	#printf "%u ==> <%s>\n" $i "${FILES[$i]}"
	file=${FILES[$i]}
	opt=${FILE_OPTS[$i]}

	option_match $opt || { let i=i+1; continue; }
	let num_done=num_done+1

	fname=$(make_fname "$file")
	#fname=$(($FILE_START_N+$i))-$fname
	fname="f-"$fname

	#echo "$file => $fname"; let i=i+1; continue;
	{
	    echo "# cat $file"
	    cat $file
	} > $fname 2>&1
	let i=i+1
    done
    [[ $opt_v -eq 1 ]] && echo "done ($num_done)"
    cd $OLDPWD
}

function sysfs_attributes()
{
    local c f line

    OUT_FILE="$TMP_DIR/s-sysfs-attr.txt"
    TMP_PFX="$OUT_FILE.$$"
    CLASSES="fc_host fc_remote_ports fc_transport fc_vports \
	    scsi_device scsi_disk scsi_generic scsi_host bsg"
    SYS_CAT="/sys/devices /sys/module/qla2xxx"
    SYS_CAT="$SYS_CAT /sys/module/qla2xxx_scst /sys/module/qla2x00tgt"
    # for "devices" dir, get lines only that matches the following pattern
    GREP_PATT_devices="host"

    #[[ $opt_v -eq 1 ]] && echo -n "collecting sysfs attributes.."
    for c in $CLASSES; do
	f=/sys/class/$c
	file1=$(echo $f/* |grep -v '*' |awk '{print $1}')
	[[ "$file1" = "" ]] && continue
	echo "-- $f --"
	if [[ -L "$file1" ]];then
	    # sym link, newer kernel (~2.6.39), all info will be captured
	    # by /sys/devices traversal
	    ls -l $f
	else
	    ls -l $f/*/device
	    SYS_CAT="$SYS_CAT $f"
	fi
    done

    let nc=0
    for f in $SYS_CAT; do
	let nc=nc+1
	bf=${f##*/}
	grep_patt=$(eval "echo \$GREP_PATT_$bf")
	[[ -z "$grep_patt" ]] && grep_patt="^"
	find $f/ -type f 2>/dev/null |grep $grep_patt |while read line; do
	    [[ ! -r "$line" ]] && continue
	    ls -l "$line" |grep -q '^.r'
	    [[ $? -ne 0 ]] && continue # write only attr
	    [[ -n "$grep_patt" ]] && {
		echo "$line" |grep -q "$grep_patt" || continue
	    }
	    echo "$line : `head -1 \"$line\" 2>/dev/null`"
	done > $TMP_PFX.$nc 2>&1 &
    done
    wait
    cat $TMP_PFX.* > $OUT_FILE
    rm $TMP_PFX.*

    cat $OUT_FILE
    rm $OUT_FILE

    #[[ $opt_v -eq 1 ]] && echo "done."
}

function final_touch()
{
    > $META_FILE
    echo "# cat $META_FILE" >> $META_FILE
    echo "qdiag.sh: version $version" >> $META_FILE
    cat $CMD_FILE >> $META_FILE
    cat $FILES_FILE >> $META_FILE
    rm $CMD_FILE $FILES_FILE
}

function zip_all()
{
    cd $TMP_BASE/..
    tar zcf - $DIR_NAME > $QDIAG_FILE
    echo "Created $QDIAG_FILE"
}

function script_done()
{
    [[ "$me_created" -eq 1 ]] && rm -rf $TMP_BASE
}

# main
read_opts "$@"

script_init

read_commands
read_files

get_files;

compute_prefix

run_commands

final_touch

[[ $opt_v -eq 1 ]] && echo

zip_all

#script_done

exit 0

# ========= All commands / files that needs to be captured goes below ========
# Syntax:
#   +[options] <command>[ ; <script-function>]
#   +[options] <file>[ ; <script-function>]
#   where:
#   [options] indicate the command-line option that is required to capture
#       the command output. ("+ 2d uname" means "uname" output is captured
#       on options -2 or -d. No option means captured all times.
#   NOTE 1: Lines starting with # are means comment.
#   NOTE 2: when adding new options below, please make sure options match with
#           the one in read_opts()

--- Commands ---
+r      ps aux
+       uname -a
+r      uptime
+       mount
+       df
+       /sbin/lsmod
+       /sbin/ifconfig
+       /sbin/lspci -nn
+3      /sbin/lspci -vvvxxxx
+4      ls -l /dev
+       ls -lR /dev/disk
+r      lsscsi
+r      dmesg
+r      ls -l /sys/class/scsi_host
+       head -1 /etc/*release
+       /usr/sbin/dmidecode
+       procinfo
+s      sysfs_attributes

--- Files ---
+       /var/log/kern.log
+       /var/log/messages
+       /var/log/syslog
+       /etc/default/grub
+       /proc/version
+       /proc/cmdline
+       /proc/cpuinfo
+r      /proc/meminfo
+       /proc/partitions
+r      /proc/buddyinfo
+r      /proc/slabinfo
+       /proc/devices
+       /proc/sys/kernel/printk

# Please retain the newline above
