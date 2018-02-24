#!/bin/sh -e

# Copyright (c) 2018-2021 Marvell.
# ACTION FILE: located in /lib/udev/
#
#

err() {
	echo "$@" >&2
	if [ -x /bin/logger ]; then
		/bin/logger -t "${0##*/}[$$]" "$@"
	elif [ -x /usr/bin/logger ]; then
		/usr//bin/logger -t "${0##*/}[$$]" "$@"
	fi
}

(( SIZE_LIMIT = 20 * 1024 *1024))

size_check()
{
    local rc
    local size
    local sz
    size=0
    ls -1 qla2xxx_fw_dump_${HOST}_* 2>/dev/null | sort -r | while read file
    do
        sz=`stat --printf="%s" $file`
        ((size += sz))
        if [ $size -gt $SIZE_LIMIT ]
        then
            err "FW dump collection has stopped for host${HOST} due to $SIZE_LIMIT bytes limit."
            err "Please save /opt/QLogic_Corporation/FW_Dumps/qla2xxx_fw_dump_${HOST}_xx to different location"
            exit 12
        fi
    done
    return 0
}


SYSFS=/sys
HOST=${FW_DUMP}
QFWD=${SYSFS}/class/fc_host/host${HOST}/device/fw_dump
DFILE_PATH=/opt/QLogic_Corporation/FW_Dumps
DFILE=${DFILE_PATH}/qla2xxx_fw_dump_${HOST}_`eval date +%Y%m%d_%H%M%S`.txt
MDFILE=${DFILE_PATH}/qla2xxx_mpi_fw_dump_${HOST}_`eval date +%Y%m%d_%H%M%S`.txt

# Verify fw_dump binary-attribute file
if ! test -f ${QFWD} ; then
	err "qla2xxx: no firmware dump file at host $HOST!!!"
	exit 1
fi

# Go with dump
mkdir -p ${DFILE_PATH}

cd /opt/QLogic_Corporation/FW_Dumps
size_check


# MPI dump
echo 9 > ${QFWD}
cat ${QFWD} > ${MDFILE}
echo 8 > ${QFWD}
if ! test -s "${MDFILE}" ; then
	rm ${MDFILE}
else
    gzip ${MDFILE}
    err "qla2xxx: MPI firmware dump saved to file ${MDFILE}.gz."
    exit 0
fi

# FW dump
echo 1 > ${QFWD}
cat ${QFWD} > ${DFILE}
echo 0 > ${QFWD}
if ! test -s "${DFILE}" ; then
	err "qla2xxx: no firmware dump file at host ${HOST}!!!"
	rm ${DFILE}
	exit 1
fi

gzip ${DFILE}
err "qla2xxx: firmware dump saved to file ${DFILE}.gz."
exit 0
