#!/bin/sh
#
# Initialize the onboard SMSC USB Hub on supported carriers.
#

#
# Only run on supported IO boards, by checking the board's ordinal.  The
# ordinal is extracted from the iv_io variable in the kernel cmdline.
#
# Also, only run on master.
#

#00035 - Sandcreek Carrier
#00058 - Sandcreek II Carrier
#00063 - Tri-R Carrier

SUPPORTED_IO_ORDS="00035 00058 00063"
LPE_MB_ORD="00023"

IO_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_io= | cut -d- -f2`
MB_ORD=`cat /proc/cmdline | tr " " "\n" | grep ^iv_mb= | cut -d- -f2`
echo "$SUPPORTED_IO_ORDS" | grep -q "$IO_ORD" || exit
cat /proc/cmdline | tr " " "\n" | grep -q master=1 || exit


# Determine which I2C Device to use
#                    LPe         Z7e
#
# SCI(35)             i2c-3       i2c-0
# SCII(58)            i2c-3/4     i2c-4
# TRIUMPH-R(63)       i2c-3       i2c-0
#
#
# if LPe, go with Lpe, else - go with Z7e

case ${IO_ORD} in
        "00035")
            case ${MB_ORD} in
                ${LPE_MB_ORD})
                    I2C_DEV="/dev/i2c-3" ;;
                *)
                    I2C_DEV="/dev/i2c-0" ;;
            esac
            ;;

        "00058")
                I2C_DEV="/dev/i2c-4"
            ;;

        "00063")
            case ${MB_ORD} in
                ${LPE_MB_ORD})
                    I2C_DEV="/dev/i2c-3" ;;
                *)
                    I2C_DEV="/dev/i2c-0" ;;
            esac
            ;;
         
        *)
            I2C_DEV="/dev/i2c-3";
            ;;
 
esac

#
# Hub is initialized to defaults as defined in the datasheet.
#
echo "Running USB hub init sequence"
I2CUSBHUB="/etc/rc.d/i2cusbhub ${I2C_DEV} 2C"
$I2CUSBHUB 00 24
$I2CUSBHUB 01 04
$I2CUSBHUB 02 14
$I2CUSBHUB 03 25
$I2CUSBHUB 04 A0
$I2CUSBHUB 05 0B
$I2CUSBHUB 06 9B
$I2CUSBHUB 07 20
$I2CUSBHUB 08 02
$I2CUSBHUB 09 00
$I2CUSBHUB 0a 00
$I2CUSBHUB 0b 00
$I2CUSBHUB 0c 01
$I2CUSBHUB 0d 32
$I2CUSBHUB 0e 01
$I2CUSBHUB 0f 32
$I2CUSBHUB 10 32
$I2CUSBHUB ff 01
