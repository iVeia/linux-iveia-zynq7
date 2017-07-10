#!/bin/sh
#
# Initialize SMSC7500 USB2.0 Ethernet Controller's MAC address on
# AEP Module.
#

#
# Only run on supported IO boards, by checking the board's ordinal.  The
# ordinal is extracted from the iv_bp variable in the kernel cmdline.
#

SUPPORTED_IO_ORDS="00052"
IO_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_bp= | cut -d- -f2`
echo "$SUPPORTED_IO_ORDS" | grep -q "$IO_ORD" || exit
#
SN=`cat /proc/cmdline | tr " " "\n" | grep ^iv_bp= | cut -d, -f2`
if [ -z "$SN" ]; then
    echo "Could not set MAC address"
    exit
fi

#
# Serial number conversion (defined in internal wiki)
#
TR_ALPHA="A-HJ-NP-Z2-9"
TR_NUM="\\000\\001\\002\\003\\004\\005\\006\\007\\010\\011\\012\\013\\014\\015\\016\\017\\020\\021\\022\\023\\024\\025\\026\\027\\030\\031\\032\\033\\034\\035\\036\\037"
SN_BYTES=`echo $SN | tr $TR_ALPHA $TR_NUM | od -b | head -1 | cut -d\  -f2-7`

SN_VAL=0
SHIFT=21
for i in $SN_BYTES; do
    SN_VAL=$((SN_VAL+(0$i<<SHIFT)));
    SHIFT=$((SHIFT-5));
done
SN_VAL2=$(((SN_VAL&0xFF0000)>>16))
SN_VAL1=$(((SN_VAL&0x00FF00)>>8))
SN_VAL0=$(((SN_VAL&0x0000FF)>>0))
MAC=`printf "00:21:68:%02X:%02X:%02X" $SN_VAL2 $SN_VAL1 $SN_VAL0`

ifconfig eth0 hw ether $MAC
