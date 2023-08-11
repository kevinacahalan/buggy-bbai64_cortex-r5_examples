## Use of &= and |= on GPIO from an R5 on the BeagleBoneAI64 breaks Linux


The code in this repo is based off https://forum.beagleboard.org/t/bbai-64-cortex-r5-gpio-toggle-experiment/32942


## Description
If I use the `&=` operator to clear bit 29 in the `GPIO_DIR45` register, and at the same time use the `|=` operator to set bits 29 in the `GPIO_SET_DATA45` and `GPIO_CLR_DATA45` registers, random files become unreadable from linux. I also found that whenever this weird bug hits, running `journal -k` will not complete and when and will end with an abort message.

## Making it hit
This folder has 6 example programs, 3 working and 3 not working. **The bug only hits if there has not been a working example run since the last board reset/powercycle**. In other words, if I run `r5_toggle4_orig-working` and then next run `r5_toggle4_broken_1_yes-and-eql_yes-or-eql` without a reset or powercycle, the broken example will seem to work and not bug out. 

Alternatively, if I first reset the board and then run `r5_toggle4_broken_1_yes-and-eql_yes-or-eql`, the bug will hit. 

1. Run `make` to build
2. Run `sudo ./startup.sh` to run the firmware


## Spotting the crazy
- Run `journal -k` and see if it aborts
- Cat random files on the system until you see some weird input/output error
- Random programs don't start
- Sometimes linux just flat out freezes


## Random notes
- bit 29 in the block 4-5 register corresponds to pin p9-14 on the BeagleBoneAI64.
- Although I have not tested, I highly suspect this issue hits with other gpio blocks and bits.
- I tested this bug on two different BBAI64's and with several different SD card of the same model and size.
- I was using linux kernel 5.10 but iirc I also hit this bug on 6.x
- I slowly narrowed this bug down after a week and half of fighting. That fact that the bug only hits if I had first not run a working firmware messed with me hard.
- The r5 firmware seems to keep running fine when the bug hits. For the examples in this repo the p9-14 pin keep toggling after linux gets messed up.



### Random stuff:

`k3conf` randomly not working after bug hit:
```
debian@BeagleBone:~/xfox_linux/r5_toggle5$ k3conf dump processor
-bash: /usr/sbin/k3conf: Input/output error

```
Example by lucky chance of `cat` run from `startup.sh` not running thanks to bug:
```
debian@BeagleBone:~/buggy-bbai64_cortex-r5_examples/r5_toggle4_broken_1_yes-and-eql_yes-or-eql$ make
arm-none-eabi-gcc -fno-exceptions -mcpu=cortex-r5 -marm -mfloat-abi=hard -ggdb -I r5 --specs=nosys.specs --specs=nano.specs -T gcc.ld -o test.elf r5/CacheP_armv7r_asm.S r5/CpuId_armv7r_asm.S r5/HwiP_armv7r_asm.S r5/MpuP_armv7r_asm.S r5/PmuP_armv7r_asm.S r5/CacheP_armv7r.c r5/MpuP_armv7r.c r5/PmuP_armv7r.c test.c -u _printf_float
arm-none-eabi-size test.elf
   text	   data	    bss	    dec	    hex	filename
  23400	   4808	    196	  28404	   6ef4	test.elf
arm-none-eabi-objdump -xd test.elf > test.elf.lst
sudo cp test.elf /lib/firmware/
#  sudo echo stop > /sys/class/remoteproc/remoteproc18/state
#  sudo echo test.elf > /sys/class/remoteproc/remoteproc18/firmware
#  sudo echo start > /sys/class/remoteproc/remoteproc18/state
#  sudo cat /sys/kernel/debug/remoteproc/remoteproc18/trace0
debian@BeagleBone:~/buggy-bbai64_cortex-r5_examples/r5_toggle4_broken_1_yes-and-eql_yes-or-eql$ sudo ./startup.sh 
sudo: unable to execute /bin/cat: Input/output error
```


Bottom 15ish lines from attempting to run `journalctl -k` after starting up a broken example:
```
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
Aug 11 20:17:20 BeagleBone kernel: sdhci-am654 4fb0000.mmc: error -84 requesting status
Aug 11 20:17:20 BeagleBone kernel: mmcblk1: recovery failed!
SIGBUS handling failed: Value too large for defined data type
Aborted
```


Can't read kernel logs with `cat`, `less`, or `more`. Sometimes by luck I can read the logs.
```
debian@BeagleBone:~/buggy-bbai64_cortex-r5_examples/r5_toggle4_broken_1_yes-and-eql_yes-or-eql$ cat /var/log/kern.log
-bash: /bin/cat: Input/output error
debian@BeagleBone:~/buggy-bbai64_cortex-r5_examples/r5_toggle4_broken_1_yes-and-eql_yes-or-eql$ less /var/log/kern.log
-bash: /usr/bin/less: Input/output error
debian@BeagleBone:~/buggy-bbai64_cortex-r5_examples/r5_toggle4_broken_1_yes-and-eql_yes-or-eql$ more /var/log/kern.log
-bash: /bin/more: Input/output error
```


This is what you see at the end of `journalctl -k` after starting up a working example:
```
Aug 11 20:40:04 BeagleBone kernel: remoteproc remoteproc18: powering up 5e00000.r5f
Aug 11 20:40:04 BeagleBone kernel: remoteproc remoteproc18: Booting fw image test.elf, size 524760
Aug 11 20:40:04 BeagleBone kernel:  remoteproc18#vdev0buffer: assigned reserved memory node vision-apps-r5f-dma-mem>
Aug 11 20:40:04 BeagleBone kernel: virtio_rpmsg_bus virtio1: rpmsg host is online
Aug 11 20:40:04 BeagleBone kernel:  remoteproc18#vdev0buffer: registered virtio1 (type 7)
Aug 11 20:40:04 BeagleBone kernel: remoteproc remoteproc18: remote processor 5e00000.r5f is now up

```