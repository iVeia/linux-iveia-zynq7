#!/bin/sh
#
# Initialize the GPIOs
#

#
# Only run on supported IO boards, by checking the board's ordinal.  The
# ordinal is extracted from the iv_io variable in the kernel cmdline.
#
# Also, only run on master.
#
SUPPORTED_IO_ORDS="00035"
IO_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_io= | cut -d- -f2`
echo "$SUPPORTED_IO_ORDS" | grep -q "$IO_ORD" || exit
cat /proc/cmdline | tr " " "\n" | grep -q master=1 || exit

#
# Initialize Sandcreek Carrier GPIOs
#
set_gpio()
{
    echo $1 > /sys/class/gpio/export
    echo $2 > /sys/class/gpio/gpio${1}/direction
}

set_gpio 219 high   # GPS_RESET#
set_gpio 220 high   # GPS_PROGSELECT#
set_gpio 224 low    # ADC1_NAPSLP

