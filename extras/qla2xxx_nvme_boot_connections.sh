#!/bin/bash
#set -x
#
# Copyright (c) 2018-2020 Marvell.
#

connect_fcnvme_luns_on_host()
{
#	logger -s "**** qla2xxx-boot-service called ****"
	INPUTFILE=$1/nvme_connect_str
	HNAME=`basename $1`
	if [ -f ${INPUTFILE} ] ; then
		HOST_TRADDR=`cat ${INPUTFILE} | awk '/FC-NVMe LPORT:.*port_id/{print $4}'` > /dev/null
		TR=`cat ${INPUTFILE} | awk '/FC-NVMe RPORT:.*port_id/{print $4}'` > /dev/null
		echo $TR | sed 's/ /\n/g' | while read TRADDR
		do
			if [ "${HOST_TRADDR}" != "" ] && [ "${TRADDR}}" != "" ] ; then
#				logger -s "${INPUTFILE}"
#				logger -s "Using host-traddr=${HOST_TRADDR} traddr=${TRADDR}"
				cmd="/usr/sbin/nvme connect-all --transport=fc --host-traddr=${HOST_TRADDR} --traddr=${TRADDR}"
				$cmd
			fi
		done
	fi
}

ROOT=1

if [ $ROOT -ne 1 ] ; then
  echo "$0 : Must be root to execute"
  exit 1
fi

for f in /sys/class/scsi_host/* ; do
   connect_fcnvme_luns_on_host $f
done
