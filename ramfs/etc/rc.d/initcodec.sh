#!/bin/sh
#
# Initialize Wolfson 8960 Audio Codec when a Triumph Carrier is attached.
#
#
# Only run on supported IO boards, by checking the board's ordinal.  The
# ordinal is extracted from the iv_bp variable in the kernel cmdline.
#

#00043 - Tri Carrier
#00063 - Tri-R Carrier

SUPPORTED_IO_ORDS="00043 00063"

IO_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_io= | cut -d- -f2`
echo "$SUPPORTED_IO_ORDS" | grep -q "$IO_ORD" || exit

MB_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_mb= | cut -d- -f2`

if [ $MB_ORD = "00023" ]; then  #LPe
    DEV=/dev/i2c-3
elif [ $MB_ORD = "00049" ]; then #Z7e
    DEV=/dev/i2c-0
else
    echo "$0 : Non-supported Main-Board"
    exit
fi

REGWIDTH=1
#    err = stereoWr(25,0xc3);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 32 C3
#    err = stereoWr(26,0x60);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 34 60
#    err = stereoWr(43,0x0A);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 56 0A
#    err = stereoWr(44,0x0A);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 58 0A
#    err = stereoWr(47,0x0C);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 5E 0C
#    err = stereoWr(45,0x80);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 5A 80
#    err = stereoWr(46,0x80);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 5C 80
#    err = stereoWr(2,0x179);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 05 79
#    err = stereoWr(3,0x179);
/etc/rc.d/i2crw -d $DEV -a 1a -r $REGWIDTH 07 79

