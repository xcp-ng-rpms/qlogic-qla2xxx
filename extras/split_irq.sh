#!/bin/bash

$qtrc

#
# Copyright (c) 2018-2020 Marvell.
#
# This script is sample script to set the IRQ of QLogic FC driver qla2xxx.
# Feel free to modify it to fit your configuration need.
#
# Each physical port of the Qlogic FC adapter has multiple interrupt vectors.
# This script assign each vector to different CPU(s) to allow the load to be
# distributed across different CPUs.  Otherwise, all interrupts, by default,
# will be routed to CPU0.
#
# NOTE: This script does NOT distribute the same interrupt across multiple CPUs.
#	Instead, it presents multiple CPUs to a specific interrupt to
#	choose from.  When an interrupt makes the selection, that interrupt
#	generally will stay on that CPU.
#



# number of cpus
NUMCPU=`cat /proc/cpuinfo  | grep processor | wc -l`

# last cpu bit
(( LCB = (1 << NUMCPU) )) #

# water mark level to choose 2 CPUs vs 1 cpu per interrupt
WML=4

# number of PCI functions.  Each PCI function has a default vector
NUMFUNCS=`cat /proc/interrupts | grep qla2xxx| grep default | wc -l `

function usage
{
    echo "$0 [-u | -l | -w ]"
    echo "no arg : split the irq "
    echo "  -u : reset smp_affinity to ffffff "
    echo "  -l : list irq "
    echo "  -w : watch irq to see where irqs are hitting"
}


function split_a
{
    echo "atio irq"
    ICPU=1
    cat /proc/interrupts | grep qla2xxx | grep -e atio_q | \
	awk -F: '{print $1}' | while read i name
    do
	if [ $ICPU -ge $LCB ] ; then
		ICPU=1
	fi

	myhex=`echo "obase=16; $ICPU" | bc`
	echo "before: $i : `cat /proc/irq/$i/smp_affinity`"
	#echo $myhex
	echo $myhex > /proc/irq/$i/smp_affinity
	echo "after : $i : `cat /proc/irq/$i/smp_affinity`"

	((ICPU = (ICPU << 2) )) #
    done

    echo "rspq irq"
    (( ICPU = 1 << 1 )) #
    cat /proc/interrupts  | grep qla2xxx | grep -e rsp_q | \
	awk -F: '{print $1}' | while read i name
    do
	if [ $ICPU -ge $LCB ] ; then
		(( ICPU = 1 << 1 )) #
	fi

	myhex=`echo "obase=16; $ICPU" | bc`
	echo "before: $i : `cat /proc/irq/$i/smp_affinity`"
	#echo $myhex
	echo $myhex > /proc/irq/$i/smp_affinity
	echo "after : $i : `cat /proc/irq/$i/smp_affinity`"

	((ICPU = (ICPU << 2) ))  #
    done
}

function split_b
{
	#atio
	cat /proc/interrupts  | grep qla2xxx | grep -e atio | awk -F: '{print $1}' | \
	while read i name
	do
	    myhex=0000ff
	    echo $myhex > /proc/irq/$i/smp_affinity
	    echo "$i : `cat /proc/irq/$i/smp_affinity`"
	done

	#rspq
	cat /proc/interrupts  | grep qla2xxx | grep -e rsp | awk -F: '{print $1}' | \
	while read i name
	do
	    myhex=00ff00
	    echo $myhex > /proc/irq/$i/smp_affinity
	    echo "$i : `cat /proc/irq/$i/smp_affinity`"
	done

}

OPTS=`getopt -o wulh? : -l help  -- "$@"`

if [ $? != 0 ] ; then
    usage
    exit 1
fi

if [ $# -eq 0 ] ; then
    # no arg provided.
    echo -e "\nPlease consider turning off irqbalance.  It interferes with this script"
    echo "# service irqbalance stop."
    echo -e "\n"
    split_a
    exit 0
fi

eval set -- "$OPTS"
while [ $# -gt 0 ] ; do
    #echo $@
    #echo "number of param $#"
    case $1 in
    (-u) # undo
	cat /proc/interrupts  | grep qla2xxx | grep -e rsp -e atio -e default | awk -F: '{print $1}' | \
	while read i
	do
	    echo 01 > /proc/irq/$i/smp_affinity
	    sync;sync;
	    echo 0ffffff > /proc/irq/$i/smp_affinity
	    cat /proc/irq/$i/smp_affinity
	done
	;;

    (-l) # list irq
	cat /proc/interrupts  | grep qla2xxx | grep -e rsp_q -e atio_q -e default

	cat /proc/interrupts  | grep qla2xxx | grep -e rsp_q -e atio_q -e default | awk -F: '{print $1}' | \
	while read i
	do
	    echo "$i: `cat /proc/irq/$i/smp_affinity`"
	done
	;;

    (-w)
	echo "Ctrl-C to break out of watch cmd."
	sleep 3
	watch -d -n 1 'cat /proc/interrupts  | grep qla'
	;;

    (--)
	# end of arg list
	;;

    (-h| --help | -?)
	    usage
	    exit 1
	    ;;
    esac;
    shift
done
