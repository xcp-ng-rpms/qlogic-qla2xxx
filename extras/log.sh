#!/bin/bash
# Copyright (c) 2018-2020 Marvell.
#
help() {
	cat <<-END
		$0	extract log/dbg messages from driver source

		$0  sort-option  [ tabulate-option ]

		sort-option is required, must be one of:
		--cat	sort by message category
		--str	sort by message string
		--id	sort by message id

		tabulate-option is optional:
		--tab	tabulate output (strip away C syntax)
	END
	exit
}

log() {
	sed -rf <(cat - <<-SED
		/ql_(log|dbg|log_pci|dbg_pci)[ \t]*\(/ {
			:a
			/\)[ \t\n]*;/ {
				s/[ \t\n]+/ /g
				s/,[ \t]*/, /g
				/, *\.\.\. *\)/d
				b
			}
			N
			b a
		}
		d
	SED
	) $@
}

tab() {
	sed -nrf <(cat - <<-TAB
		s/^(.+)\((.+), (.+), (0x[0-9a-fA-F]+), (.+)\);$/\2 \4 \5/p
	TAB
	)
}

ctg() {
	$0 --cat --tab | awk -F '\t' '{ print $1 }' | sort -u
	exit
}

case $1 in
	"--cat"	) sort="-t, -k1,1" ;;
	"--str"	) sort="-t, -k4,4" ;;
	"--id"	) sort="-t, -k3,3 -g" ;;
	"cat"   ) ctg ;;
	*	) help ;;
esac
case $2 in
	"--tab"	) log *.[ch] | sort $sort | tab ;;
	""	) log *.[ch] | sort $sort ;;
	*	) help ;;
esac
