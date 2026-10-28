/******************************************************************************/
/*
 * File:    StartupText.cpp
 * Authors: Luke de Munk
 * Version: 1.0.0
 *
 * Brief:   Prints a realistic embedded Linux startup log as hexadecimal bytes.
 *
 *          More information: https://github.com/LukedeMunk/root-node
 */
/******************************************************************************/
#include "StartupText.h"

#include <pgmspace.h>

static const char STARTUP_LOG_BEFORE_STATUS[] PROGMEM = R"boot(U-Boot 1.1.3 (Oct 12 2026 - 19:42:17)

Board: SmartLight Gateway rev 2.1
SoC:   Ralink RT5350F @ 360 MHz
DRAM:  32 MiB
Flash: 4 MiB
In:    serial
Out:   serial
Err:   serial
Net:   esp-wlan0
Hit any key to stop autoboot:  0
Loading kernel from flash at 0x00100000 ...
   Image Name:   SmartLight Embedded Linux
   Image Type:   MIPS Linux Kernel Image (gzip compressed)
   Data Size:    362724 Bytes
   Load Address: 0x40100000
   Entry Point:  0x40100000
   Verifying Checksum ... OK
   Uncompressing Kernel Image ... OK
Starting kernel ...

[    0.000000] Linux version 4.14.275-smartlight lite (builder@firmware-ci) (gcc 10.3.0) #18 Mon Oct 12 19:41:08 CEST 2026
[    0.000000] CPU0 revision is: 0001964c (MIPS 24KEc)
[    0.000000] bootconsole [early0] enabled
[    0.000000] Kernel command line: console=ttyS0,115200 root=/dev/mtdblock3 rootfstype=squashfs,jffs2
[    0.000000] Memory: 27612K/32768K available (2896K kernel code, 214K rwdata, 812K rodata, 168K init)
[    0.009000] Calibrating delay loop... 239.61 BogoMIPS (lpj=1198080)
[    0.037000] gpiochip_add_data: registered GPIOs 0 to 27 on device: rt2880-gpio
[    0.052000] clocksource: Switched to clocksource MIPS
[    0.061000] NET: Registered protocol family 2
[    0.083000] TCP: Hash tables configured (established 1024 bind 1024)
[    0.114000] squashfs: version 4.0 (2009/01/31) Phillip Lougher
[    0.121000] jffs2: version 2.2. (NAND) (SUMMARY) (ZLIB) (RTIME)
[    0.144000] Serial: 8250/16550 driver, 1 ports, IRQ sharing disabled
[    0.152000] console [ttyS0] enabled
[    0.158000] bootconsole [early0] disabled
[    0.166000] spi-nor spi0.0: gd25q32 (4096 Kbytes)
[    0.173000] 5 fixed-partitions partitions found on MTD device spi0.0
[    0.181000] Creating 5 MTD partitions on "spi0.0":
[    0.203000] 0x000000020000-0x000000100000 : "kernel"
[    0.211000] 0x000000100000-0x000000300000 : "rootfs"
[    0.234000] 0x0000002c0000-0x000000300000 : "rootfs_data"
[    0.242000] 0x000000300000-0x000000400000 : "spiffs"
[    0.251000] rt2800_wmac 10180000.wmac: MAC address 8c:aa:b5:7b:e0:a8
[    0.260000] rt2800_wmac 10180000.wmac: RF calibration data loaded
[    0.276000] NET: Registered protocol family 17
[    0.293000] 8021q: 802.1Q VLAN Support v1.8
[    0.301000] VFS: Mounted root (squashfs filesystem) readonly on device 31:3.
[    0.327000] Run /sbin/init as init process
[    0.344000] init: Console is alive
[    0.351000] init: - watchdog -
[    0.381000] init: - preinit -
[    0.458000] mount_root: switching to jffs2 overlay
[    0.526000] procd: - init -
[    0.568000] urngd: v1.0.2 started.
[    0.579000] smartlight-init[418]: checking persistent configuration
[    0.590000] smartlight-init[418]: configuration schema v3 accepted
[    0.603000] smartlight-init[418]: initializing EEPROM
[    0.616000] smartlight-init[418]: external filesystem ready
[    0.629000] smartlight-init[418]: loading encrypted web-user database
[    0.642000] smartlight-init[418]: loaded 2 web-user records
[    0.647000] smartlight-init[418]: serial console )boot";

static const char SERIAL_CONSOLE_ENABLED[] PROGMEM = "enabled";
static const char SERIAL_CONSOLE_DISABLED[] PROGMEM = "disabled";

static const char STARTUP_LOG_BEFORE_SECRET[] PROGMEM = R"boot(
[    0.651000] smartlight-init[418]: serial access controlled by runtime debug policy
[    0.655000] smartlight-init[418]: command 'debug --on' starts serial authentication
[    0.659000] smartlight-init[418]: serial authentication handler ready
[    0.663000] smartlight-init[418]: warning weak serial password configured, please change
[    0.667000] smartlight-init[418]: serial debug password: )boot";

static const char STARTUP_LOG_AFTER_SECRET[] PROGMEM = R"boot(
[    0.669000] smartlight-init[418]: initializing Linux filesystem
[    0.682000] smartlight-init[418]: registered 22 filesystem nodes
[    0.695000] smartlight-init[418]: registered 3 file mappings
[    0.708000] smartlight-init[418]: limited serial command registry initialized
[    0.722000] smartlight-init[418]: serial console attached to /dev/ttyS0
[    0.737000] network: loading interface configuration
[    0.762000] network: interface 'wlan' is enabled
[    0.798000] wlan0: authenticated
[    0.834000] wlan0: associated
[    0.872000] udhcpc: broadcasting discover
[    0.903000] udhcpc: broadcasting select for 192.168.1.143
[    0.916000] udhcpc: lease of 192.168.1.143 obtained, lease time 86400
[    0.929000] network: setting default route via 192.168.1.1
[    0.941000] network: adding DNS server 192.168.1.1
[    0.953000] smartlight-mdns[506]: publishing smartlight.local
[    0.966000] smartlight-httpd[514]: loading static assets from flash
[    0.978000] smartlight-httpd[514]: session store initialized
[    0.991000] smartlight-httpd[514]: listening on 0.0.0.0:80
[    1.004000] smartlightd[521]: firmware version 1.0.0
[    1.017000] smartlightd[521]: board profile 'SmartLight Gateway rev 2.1'
[    1.029000] smartlightd[521]: GPIO2 configured as PWM output
[    1.042000] smartlightd[521]: restored LED brightness 1023
[    1.055000] smartlightd[521]: REST API initialized
[    1.068000] smartlightd[521]: file service initialized
[    1.080000] smartlightd[521]: serial CLI initialized
[    1.093000] smartlightd[521]: startup self-test passed
[    1.106000] procd: instance smartlightd::instance1 entered running state
[    1.119000] procd: instance smartlight-httpd::instance1 entered running state
[    1.132000] cron[548]: crond (busybox 1.35.0) started, log level 5
[    1.145000] logd[552]: ring buffer size 16384 bytes
[    1.157000] random: crng init done
[    1.169000] smartlight-init[418]: boot completed in 1169 ms

WARNING!
The use of this serial connection is strictly prohibited and constitutes a violation of the service agreements.
It is intended for use only by authorized technicians during maintenance or diagnostic procedures.
Unauthorized use may result in system malfunction, operational failure and will void any applicable warranty.
WARNING!

SmartLight Embedded Linux Lite Distro 1.0.0
smartlight ttyS0
)boot";

static void printHexByte(uint8_t value, uint8_t& bytesOnLine) {
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
    bytesOnLine++;
    if (bytesOnLine == STARTUP_BYTES_PER_LINE) {
        Serial.println();
        bytesOnLine = 0;
    } else {
        Serial.print(' ');
    }
}

static void printFlashBytes(const char* text, uint8_t& bytesOnLine) {
    for (size_t position = 0;; position++) {
        uint8_t value = pgm_read_byte(text + position);
        if (value == 0) {
            return;
        }
        printHexByte(value, bytesOnLine);
    }
}

static void printStringBytes(const char* text, uint8_t& bytesOnLine) {
    for (size_t position = 0; text[position] != '\0'; position++) {
        printHexByte(static_cast<uint8_t>(text[position]), bytesOnLine);
    }
}

/******************************************************************************/
/*!
  @brief    Prints a realistic boot log as fixed-width hexadecimal byte rows.
  @param    hiddenMessage       Debug information embedded in the log stream
  @param    isSerialConsoleEnabled Persisted serial console state
  @returns  bool                True after the complete log was emitted
*/
/******************************************************************************/
bool printStartupText(const char* hiddenMessage, bool isSerialConsoleEnabled) {
    uint8_t bytesOnLine = 0;

    printFlashBytes(STARTUP_LOG_BEFORE_STATUS, bytesOnLine);
    printFlashBytes(isSerialConsoleEnabled ? SERIAL_CONSOLE_ENABLED :
                    SERIAL_CONSOLE_DISABLED, bytesOnLine);
    printFlashBytes(STARTUP_LOG_BEFORE_SECRET, bytesOnLine);
    printStringBytes(hiddenMessage, bytesOnLine);
    printFlashBytes(STARTUP_LOG_AFTER_SECRET, bytesOnLine);
    if (bytesOnLine != 0) {
        Serial.println();
    }
    return true;
}
