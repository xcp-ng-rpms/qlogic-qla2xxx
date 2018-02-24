#!/bin/bash

# Cavium ISP2xxx/ISP4xxx device driver build script
# Copyright (C) 2003-2016 QLogic Corporation
# Copyright (C) 2016-2017 Cavium Inc
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

#set -x

UNAME=`uname -a`
K_VERSION=`uname -r`
K_LIBS=/lib/modules/${K_VERSION}
K_BUILD_DIR=${K_LIBS}/build
K_SOURCE_DIR=${K_LIBS}/source
NUMCPU=`cat /proc/cpuinfo | grep processor | wc -l`
K_THREADS=$NUMCPU

SLES=/etc/S[uU]SE-brand
RHEL=/etc/redhat-release
UBUNTU=/etc/lsb-release
OE=/etc/openEuler-release
KYLIN=/etc/kylin-release
UOS=/etc/UnionTech-release

UDEV_RULE_DIR=/etc/udev/rules.d/
UDEV_RULE_FILE=99-qla2xxx.rules
UDEV_NVME_RULE_FILE=99-nvme-fc.rules
UDEV_SCRIPT_DIR=/lib/udev
UDEV_SCRIPT=qla2xxx_udev.sh
UDEV_EARLY_RULE=/etc/udev/rules.d/05-udev-early.rules
UDEV_TMP_RULE=/tmp/tmp.rules

# FC-NVMe scripts
QL_BOOT_SCRIPT=qla2xxx_nvme_boot_connections.sh
QL_BOOT_SCRIPT_DIR=/usr/sbin/qla2xxx
QL_BOOT_SERVICE=qla2xxx-nvmefc-boot-connection.service
QL_BOOT_HOST_CONNECTION_SERVICE=qla2xxx-nvmefc-connect@.service
QL_BOOT_SERVICE_DIR=/usr/lib/systemd/system/
QL_BOOT_DRACUT_CONF=70-qla2xxx-nvmefc-autoconnect.conf
QL_BOOT_DRACUT_DIR=/usr/lib/dracut/dracut.conf.d/
QL_BOOT_UDEV_RULE_DIR=/usr/lib/udev/rules.d/
QL_UDEV_NVME_RULE_FILE=70-qla2xxx-nvmefc-autoconnect.rules

INSTALL_QLA2XXX_AUTO_CONNECT=1
if [ -f /usr/lib/systemd/system/nvmefc-boot-connections.service ]; then
       INSTALL_QLA2XXX_AUTO_CONNECT=0
fi

# Determine udev control utility to reload rules
which udevadm 1>/dev/null 2>&1
if [ $? -eq 0 ]
then
	RELOAD_RULES='udevadm control --reload-rules'
else
	RELOAD_RULES='udevcontrol reload_rules'
fi

BOOTDIR="/boot"

Q_NODE=QLA2XXX
MODULE=qla2xxx
K_INSTALL_DIR=${K_LIBS}/extra/
SPARSE_ARGS=""

if test -f ${SLES} ; then
	OLD_K_INSTALL_DIR=${K_LIBS}/extra/
	K_INSTALL_DIR=${K_LIBS}/updates/
fi

###
# drv_build -- Generic 'make' command for driver
#	$1 -- directive
#

set_variables() {
	Q_NODE=QLA2XXX
	MODULE=qla2xxx
	TCM_MODULE=tcm_qla2xxx
}

drv_build() {
	test -z "$1" && return 1

	# Go with build...
	if test -f ${SLES} ; then
		# SuSE -------------------------------------------------------
		make -j${K_THREADS} -C ${K_SOURCE_DIR} O=${K_BUILD_DIR} ${SPARSE_ARGS} M=$PWD $1
	else
		# Redhat -----------------------------------------------------
		make -j${K_THREADS} -C ${K_BUILD_DIR} ${SPARSE_ARGS} M=$PWD $1
	fi
}

fcnvme_discovery_install()
{
	echo "${Q_NODE} -- Installing dracut conf file for FC-NVMe LUN discovery at boot time..."
	cp -f ./extras/${QL_BOOT_DRACUT_CONF} ${QL_BOOT_DRACUT_DIR}

	echo "${Q_NODE} -- Installing FC-NVMe connection system service ..."
	cp -f ./extras/${QL_BOOT_SERVICE} ${QL_BOOT_SERVICE_DIR}
	cp -f ./extras/${QL_BOOT_HOST_CONNECTION_SERVICE} ${QL_BOOT_SERVICE_DIR}

	echo "${Q_NODE} -- Installing FC-NVMe connection udev Rule ..."
	cp -f ./extras/${QL_UDEV_NVME_RULE_FILE} ${QL_BOOT_UDEV_RULE_DIR}/${QL_UDEV_NVME_RULE_FILE}

	if [ ! -d ${QL_BOOT_SCRIPT_DIR} ]; then
		echo "${QL_BOOT_SCRIPT_DIR} does not exists.. Create it"
		mkdir -p ${QL_BOOT_SCRIPT_DIR}
	fi

	echo "${Q_NODE} -- Installing FC-NVMe script ..."
	cp -f ./extras/${QL_BOOT_SCRIPT} ${QL_BOOT_SCRIPT_DIR}/${QL_BOOT_SCRIPT}

	echo "${Q_NODE} -- Installing udev rule for FC-NVME discovery"
	cp -f ./extras/${UDEV_NVME_RULE_FILE} ${UDEV_RULE_DIR}/${UDEV_NVME_RULE_FILE}
    chmod 644 ${UDEV_RULE_DIR}/${UDEV_NVME_RULE_FILE}

	echo "${Q_NODE} -- Enable ${QL_BOOT_SERVICE}..."
	systemctl enable ${QL_BOOT_SERVICE}

	if [ ! -e ${QL_BOOT_UDEV_RULE_DIR}/${QL_UDEV_NVME_RULE_FILE} ]; then
		echo "${QL_BOOT_UDEV_RULE_DIR} not copied to ${QL_UDEV_NVME_RULE_FILE}"
#	else
#			echo "${QL_BOOT_UDEV_RULE_DIR} copied to ${QL_UDEV_NVME_RULE_FILE}"
	fi

	if [ ! -e ${QL_BOOT_SCRIPT_DIR}/${QL_BOOT_SCRIPT} ]; then
		echo "${QL_BOOT_SCRIPT} not copied to ${QL_BOOT_SCRIPT_DIR}"
#	else
#		echo "${QL_BOOT_SCRIPT} copied to ${QL_BOOT_SCRIPT_DIR}"
	fi

	if [ ! -e ${QL_BOOT_DRACUT_DIR}/${QL_BOOT_DRACUT_CONF} ]; then
		echo "${QL_BOOT_DRACUT_CONF} not copied to ${QL_BOOT_DRACUT_DIR}"
#	else
#		echo "${QL_BOOT_DRACUT_CONF} copied to ${QL_BOOT_DRACUT_DIR}"
	fi
}

udev_install()
{
	RET=0
	diff ./extras/${UDEV_RULE_FILE} ${UDEV_RULE_DIR}/${UDEV_RULE_FILE} &> /dev/null
	RET=$?
	diff ./extras/${UDEV_SCRIPT} ${UDEV_SCRIPT_DIR}/${UDEV_SCRIPT} &> /dev/null
	(( RET += $? ))

	diff ./extras/${UDEV_NVME_RULE_FILE} ${UDEV_RULE_DIR}/${UDEV_NVME_RULE_FILE} &> /dev/null
	(( RET += $? ))

	if [ $RET -ne 0 ]; then
		echo "${Q_NODE} -- Installing udev rule to capture FW dump..."
		cp ./extras/${UDEV_RULE_FILE} ${UDEV_RULE_DIR}/${UDEV_RULE_FILE}
        chmod 644 ${UDEV_RULE_DIR}/${UDEV_RULE_FILE}

        [ -d ${UDEV_SCRIPT_DIR} ] || mkdir -p ${UDEV_SCRIPT_DIR}
		cp ./extras/${UDEV_SCRIPT} ${UDEV_SCRIPT_DIR}/${UDEV_SCRIPT}

		# comment out the modules ignore_device rule
		if [ -e ${UDEV_EARLY_RULE} ]; then
			cp ${UDEV_EARLY_RULE} ${UDEV_EARLY_RULE}.bak
			cat ${UDEV_EARLY_RULE} | sed "s/\(^SUBSYSTEM==\"module\".*OPTIONS=\"ignore_device\".*\)/#\1/" > ${UDEV_TMP_RULE}
			if [ -s ${UDEV_TMP_RULE} ]; then
				mv -f ${UDEV_TMP_RULE} ${UDEV_EARLY_RULE}
			fi
		fi
		$RELOAD_RULES
	else
		echo "${Q_NODE} -- udev rules already installed"
	fi
}

udev_remove()
{
	rm -f ${UDEV_RULE_DIR}/${UDEV_RULE_FILE} &> /dev/null
	rm -f ${UDEV_RULE_DIR}/${UDEV_NVME_RULE_FILE} &> /dev/null
	rm -f ${UDEV_SCRIPT_DIR}/${UDEV_SCRIPT} &> /dev/null
	if [ -e ${UDEV_EARLY_RULE} ]; then
		cat ${UDEV_EARLY_RULE} |sed "s/\#\(SUBSYSTEM==\"module\".*OPTIONS=\"ignore_device\".*\)/\1/" > ${UDEV_TMP_RULE}
		if [ -s ${UDEV_TMP_RULE} ]; then
			echo "${Q_NODE} -- Removing FW capture udev rule..."
			mv -f ${UDEV_TMP_RULE} ${UDEV_EARLY_RULE}
			$RELOAD_RULES
		fi
	fi
}

fcnvme_discovery_remove()
{
	if [ -f ${QL_BOOT_SERVICE_DIR}/${QL_BOOT_SERVICE} ]; then
		echo "${Q_NODE} -- Removing autoconnect scripts... "
		rm -f ${QL_BOOT_SCRIPT_DIR}/${QL_BOOT_SCRIPT} &> /dev/null
		rm -f ${QL_BOOT_DRACUT_DIR}/${QL_BOOT_DRACUT_CONF} &> /dev/null
		rm -f ${QL_BOOT_UDEV_RULE_DIR}/${QL_UDEV_NVME_RULE_FILE} &> /dev/null
		rm -f ${QL_BOOT_SERVICE_DIR}/${QL_BOOT_SERVICE} &> /dev/null
		rm -f ${QL_BOOT_SERVICE_DIR}/${QL_BOOT_HOST_CONNECTION_SERVICE} &> /dev/null
		rm -f ${UDEV_RULE_DIR}/${UDEV_NVME_RULE_FILE} &> /dev/null
		rm -rf ${QL_BOOT_SCRIPT_DIR} &> /dev/null
	fi
}

###
# drv_install -- Generic steps for installation
#
drv_install() {
	if test $EUID -ne 0 ; then
		echo "${Q_NODE} -- Must be root to install..."
		return 1
	fi

	if test -f ${SLES}; then
		QLA2XXX_RPM=qlgc-qla2xxx-kmp-default
	fi

	if test -f ${RHEL}; then
		QLA2XXX_RPM=kmod-qlgc-qla2xxx
	fi

	if [ ! -z "$QLA2XXX_RPM" ] && rpm --quiet -q $QLA2XXX_RPM; then
		echo
		echo "ERROR: RPM $QLA2XXX_RPM conflicts with this package."
		echo "ERROR: Please remove the RPM prior to this install."
		exit 1
	fi

	#backup all modules except the one in default path
	for module in `find /lib/modules/$K_VERSION -name $MODULE.ko -o -name $TCM_MODULE.ko`
	do
		echo $module | grep "scsi" >& /dev/null
		if [ $? -ne 0 ]; then
			mv $module $module.org
		fi
	done

	echo "${Q_NODE} -- Installing the $MODULE modules to ${K_INSTALL_DIR}..."
	install -d -o root -g root ${K_INSTALL_DIR}
	install -o root -g root -m 0644 *.ko ${K_INSTALL_DIR}

	# depmod
	/sbin/depmod -a

	#install the udev rules to capture FW dump
	if [ -f ./qla2xxx.ko ]; then
		udev_install
	fi
}

build_ramdisk () {
	echo "${Q_NODE} -- Rebuilding INITRD image..."
	if test -f ${SLES} ; then
		if [ ! -f ${BOOTDIR}/initrd-${K_VERSION}.bak ]; then
			cp ${BOOTDIR}/initrd-${K_VERSION} ${BOOTDIR}/initrd-${K_VERSION}.bak
		fi

		if which mkinitrd 1>/dev/null 2>&1; then
			mkinitrd -k /boot/vmlinuz-${K_VERSION} -i /boot/initrd-${K_VERSION} >& /dev/null
		else
			dracut -f --add-drivers qla2xxx --kver ${K_VERSION} >& /dev/null
		fi
	elif test -f "${RHEL}"; then
		# Check if it is RHEL6
		#REDHAT_REL=`cat ${RHEL} | cut -d " " -f 7 | cut -d . -f 1`
		REDHAT_REL=`grep "RHEL_MAJOR" /usr/include/linux/version.h | sed -e 's/.*MAJOR \([0-9]\)/\1/'`
		echo "REDHAT RELEASE $REDHAT_REL"
		if [ "$REDHAT_REL" -le 5 ]; then
			if [ ! -f ${BOOTDIR}/initrd-${K_VERSION}.bak.img ]; then
				cp ${BOOTDIR}/initrd-${K_VERSION}.img ${BOOTDIR}/initrd-${K_VERSION}.bak.img
			fi
			mkinitrd -f /boot/initrd-${K_VERSION}.img ${K_VERSION} >& /dev/null
		else
			if [ ! -f ${BOOTDIR}/initramfs-${K_VERSION}.bak.img ]; then
				cp ${BOOTDIR}/initramfs-${K_VERSION}.img ${BOOTDIR}/initramfs-${K_VERSION}.bak.img
			fi
			dracut --force /boot/initramfs-${K_VERSION}.img $K_VERSION >& /dev/null
		fi
	elif test -f "${OE}" -o -f "${KYLIN}" -o -f "${UOS}"; then
		## Check for openEuler, Kylin and UOS  Linux Distros
		if [ ! -f ${BOOTDIR}/initramfs-${K_VERSION}.bak.img ]; then
			cp ${BOOTDIR}/initramfs-${K_VERSION}.img ${BOOTDIR}/initramfs-${K_VERSION}.bak.img
		fi
		dracut --force /boot/initramfs-${K_VERSION}.img $K_VERSION >& /dev/null
	elif test -f "${UBUNTU}"; then
                if [ ! -f ${BOOTDIR}/initrd.img-${K_VERSION}.bak.img ]; then
                        cp ${BOOTDIR}/initrd.img-${K_VERSION} ${BOOTDIR}/initrd.img-${K_VERSION}.bak
                fi
                update-initramfs -u -k ${K_VERSION} >& /dev/null
                update-grub2
	else
		ram_img=${BOOTDIR}/initramfs-${K_VERSION}.img # RH style
		if [ ! -f $ram_img ]; then
			ram_img=${BOOTDIR}/initrd-${K_VERSION} # SUSE style
		fi
		if [ -f "$ram_img" ]; then
			if [ ! -f "${ram_img}.bak" ]; then
				cp $ram_img ${ram_img}.bak
			fi
		else
			echo "WARNING: init ram image is not known, no backup created."
		fi
		if [ "`type -p dracut`" == "" ]; then
			echo "ERROR: dracut not found, init ram image not rebuilt."
			return 1
		fi
		echo "Creating new ram image using dracut.."
		dracut --force $ram_img $K_VERSION >& /dev/null

	fi
}

run_sparse () {
	which sparse 1>/dev/null 2>&1
	if [ $? -ne 0 ]
	then
		echo "Cannot find sparse executable in PATH."
		exit 1
	fi

	echo "${Q_NODE} -- Performing static analysis on $MODULE with sparse."
	drv_build clean

	SPARSE_ARGS="$1"
	drv_build ${SPARSE_ARGS} modules
}

###
#
#
shopt -s extglob
case "$1" in
    -h | help)
	echo "Cavium Corporation -- driver build script"
	echo "  build.sh <directive>"
	echo ""
	echo "   # cd <driver source>"
	echo "   # ./extras/build.sh"
	echo ""
	echo "    Build the driver sources based on the standard"
	echo "    SLES10/SLES11/RHEL5/RHEL6 build environment."
	echo ""
	echo "   # ./extras/build.sh clean"
	echo ""
	echo "    Clean driver source directory of all build files (i.e. "
	echo "    *.ko, *.o, etc)."
	echo ""
	echo "   # ./extras/build.sh new"
	echo ""
	echo "    Rebuild the driver sources from scratch."
	echo "    This is essentially a shortcut for:"
	echo ""
	echo "        # ./build.sh clean"
	echo "        # ./build.sh"
	echo ""
	echo "   # ./extras/build.sh install"
	echo ""
	echo "     Build and install the driver module files."
	echo "     This command performs the following:"
	echo ""
	echo "        1. Builds the driver .ko files."
	echo "        2. Copies the .ko files to the appropriate "
	echo "           /lib/modules/... directory."
	echo ""
	echo "   # ./extras/build.sh remove"
	echo ""
	echo "     Remove/uninstall the driver module files."
	echo "     This command performs the following:"
	echo ""
	echo "        1. Uninstalls the driver .ko files from appropriate."
	echo "           /lib/modules/... directory."
	echo "        2. Rebuilds the initrd image with the /sbin/mk_initrd"
	echo "           command."
	echo ""
	echo "   # ./extras/build.sh initrd"
	echo ""
	echo "     Build, install, and update the initrd image."
	echo "     This command performs the following:"
	echo ""
	echo "        1. All steps in the 'install' directive."
	echo "        2. Rebuilds the initrd image with the /sbin/mk_initrd"
	echo "           command."
	echo ""
	echo "   # ./extras/build.sh install_fcnvme_scripts"
	echo ""
	echo "     Install udev rule for FC-NVME disovery and "
	echo "     boot time auto-connection service/scripts. "
	echo ""
	echo "   # ./extras/build.sh remove_fcnvme_scripts"
	echo ""
	echo "     Remove udev rule for FC-NVME Disovery and "
	echo "     boot time auto-connection service/scripts. "
	echo ""
	echo "   # ./extras/build.sh analyze [--what|--clean|--local]"
	echo ""
	echo "     Perform Coverity source code analysis."
	echo "        --what   show [branch]->stream mapping"
	echo "        --clean  remove previous/incremental data"
	echo "        --local  perform analysis only (do not commit)"
	echo ""
	;;
    -i | install)
	set_variables
	echo "${Q_NODE} -- Building the $MODULE driver..."
	drv_build modules
	drv_install
	if [ $INSTALL_QLA2XXX_AUTO_CONNECT -eq 1 ]; then
		fcnvme_discovery_install
	fi
	;;
    -r | remove)
	set_variables
	echo "${Q_NODE} -- Removing the $MODULE driver..."
	if test -f ${SLES} ; then
		rm -f ${OLD_K_INSTALL_DIR}/${TCM_MODULE}.ko
		rm -f ${OLD_K_INSTALL_DIR}/${MODULE}.ko
	fi
	rm -f ${K_INSTALL_DIR}/${TCM_MODULE}.ko
	if  [ -f ${K_INSTALL_DIR}/$MODULE.ko ]; then
		rm ${K_INSTALL_DIR}/$MODULE.ko
		/sbin/depmod -a
		build_ramdisk
		if [ "$Q_NODE" == "QLA2XXX" ]; then
			udev_remove
			fcnvme_discovery_remove
		fi
	fi
	;;
    install_fcnvme_scripts)
	if [ $INSTALL_QLA2XXX_AUTO_CONNECT -eq 1 ]; then
		fcnvme_discovery_install
	fi
	;;
    remove_fcnvme_scripts)
	fcnvme_discovery_remove
	;;
    initrd)
	set_variables
	echo "${Q_NODE} -- Building the $MODULE driver..."
	drv_build modules
	drv_install
	build_ramdisk
	;;
    clean)
	echo "${Q_NODE} -- Cleaning driver build directory..."
	drv_build clean
	;;
    new)
	set_variables
	echo "${Q_NODE} -- Building the $MODULE driver..."
	drv_build clean
	drv_build modules
	;;
    analyze)
	cov_analyze.sh $2
	;;
    sparse)
	run_sparse "C=2"
	;;
    sparse_endian)
	run_sparse "C=2 CF=\"-D__CHECK_ENDIAN__\""
	;;
    !())
	echo "Unknown parameter $1"
	exit 1
	;;
    *)
	set_variables
	echo "${Q_NODE} -- Building the $MODULE driver..."
	drv_build modules
	;;
esac
