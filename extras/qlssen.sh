#
# Copyright (c) 2018-2020 Marvell.
#
#!/bin/bash

VERSION=4.0
MODULE=qla2xxx
MSSPARAM=ql2xsmartsan
MFEPARAM=ql2xfdmienable

SYSFS_MODULE=/sys/module/${MODULE}
SYSFS_DRVRVERS=${SYSFS_MODULE}/version
SYSFS_SSENABLE=${SYSFS_MODULE}/parameters/${MSSPARAM}

STATE=1
NOMAKE=

MODPROBE_DIR=/etc/modprobe.d
CONF=${MODPROBE_DIR}/${MODULE}.conf
TEMP=${MODPROBE_DIR}/temp.conf

EXIT=

verify_driver_loaded() {
	if [ ! -d ${SYSFS_MODULE} ]; then
		echo "${MODULE} not loaded"
		if [ $1 ]; then
			exit 1
		fi
		return 1
	fi
	return 0
}

verify_parameter_exists() {
	if [ ! -f ${SYSFS_SSENABLE} ]; then
		echo "${MSSPARAM} not supported by this ${MODULE}"
		if [ $1 ]; then
			exit 1
		fi
		return 1
	fi
	return 0
}

show_help() {
	cat <<-END
	$0: ${MODULE} ${MSSPARAM} enabler

	USAGE: $0 [PARAMETERS...] | [OPTIONS...]

	PARAMETERS:
		1|on		enable ${MSSPARAM} in ${CONF} (default)
		0|off		disable ${MSSPARAM} in ${CONF}
		nomake		do not build initrd image

	OPTIONS (if present will override PARAMETERS):
		help		show help text for this script
		this		show version of this script
		conf		show ${CONF}
		state		show ${MSSPARAM} of loaded ${MODULE}
		driver		show version of loaded ${MODULE}
		modinfo		show modinfo for ${MODULE}

	NOTES:
		Any previous contents of ${CONF} are preserved.
		If ${CONF} was non-existent, it is now created.
		If it was previously empty, it is now filled in.
		If it contained no options line, an options line is now added.
		If ${MSSPARAM} had no instances, a single instance of it is now added.
		If ${MSSPARAM} had multiple instances, they are now reduced to a single instance.
		The instance of ${MSSPARAM} will now assigned the value specified.
	END
	exit 0
}

show_conf() {
	verify_driver_loaded && verify_parameter_exists
	echo ${CONF}:
	cat ${CONF} 2>/dev/null
	EXIT=0
}

show_state() {
	verify_driver_loaded exit
	verify_parameter_exists exit
	path=${SYSFS_SSENABLE}
	echo -n $path:" "
	cat $path 2>/dev/null
	EXIT=0
}

show_driver() {
	verify_driver_loaded exit
	verify_parameter_exists
	path=${SYSFS_DRVRVERS}
	echo -n $path:" "
	cat $path 2>/dev/null
	EXIT=0
}

show_modinfo() {
	verify_driver_loaded exit
	modinfo ${MODULE} 2>/dev/null
	exit 0
}

show_version() {
	echo $0: version ${VERSION}
	EXIT=0
}

say_what() {
	echo What? $@
	EXIT=0
}

for x in $@ ; do
	case $x in
		help    ) show_help ;;
		this    ) show_version ;;
		conf    ) show_conf ;;
		state   ) show_state ;;
		driver  ) show_driver ;;
		modinfo ) show_modinfo ;;
		nomake  ) NOMAKE=$x ;;
		1|on    ) STATE=1 ;;
		0|off   ) STATE=0 ;;
		*       ) say_what $x ;;
	esac
done

if [ $EXIT ]; then exit $EXIT; fi

parse() {
	awk -f <(cat - <<-AWK
		NF < 3 { next }

		/options/ {
			del = 0
			for (i = 3; i <= NF; i++) {
				if (\$i ~ "^${MSSPARAM}") {
					\$i = ""
					del++
				} else if (\$i ~ "^${MFEPARAM}") {
					fdmi = \$i
					\$i = ""
					del++
				}
			}
			if (NF - del < 3)
				next
		}

		{ print }

		END {
			if (fdmi) {
				if (state)
					fdmi = "${MFEPARAM}=1"
				print "options qla2xxx " fdmi
			}
			print "options qla2xxx ${MSSPARAM}=" state
		}
	AWK
	) $@
}

if [ ! -f ${CONF} ] ; then
	touch ${CONF}
fi
if [ ${STATE} -gt 1 ] ; then
	STATE=1
fi

parse -v state=${STATE} ${CONF} > ${TEMP}
mv -f ${TEMP} ${CONF}
echo "${CONF}:"
cat ${CONF}

if [ -z ${NOMAKE} ] ; then
	extras/build.sh initrd
	exit 2
else
	echo "${MODULE} NOT rebuilding INITRD image."
	exit 3
fi
