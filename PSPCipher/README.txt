PSPCipher是一个PRX解密器，同现有的pspdecrypt.prx相比它是完全从6.20 mesg_led_02g.prx中逆向工程而来的，同6.20固件的解密程序几乎完全一样。从理论上说能比使用pspdecrypt.prx的edecrypt，prxdecrypter，isotool们解密更多种类的prx。

使用方法：
把要解密的PRX保存到ms0:/ENC/EBOOT.BIN，启动程序后将自动解密到ms0:/DEC/EBOOT.BIN 。

当前版本: 0.0

目前只加入了很小一部分的key, (0xd91613f0, 0xd91612f0, 0x2e5e10f0, 0x2e5e12f0)，因为目前只是技术验证阶段。然而请注意目前除PSPCipher还没有别的解密器能解密0x2e5e10f0 PRX。这个key常常被用来加密游戏补丁。也就是说你在自制系统上能用游戏补丁了。方法如下，以噬神者1.01补丁为例：

PBOOT.PBP用unpack-pbp解压后可得个DATA.PSP和PARAM.SF0，前者是个用0x2E5E10F0加密的EBOOT.BIN，是补丁后的主程序，后者是补丁后的游戏信息文件，更新了游戏版本信息。

把DATA.PSP改名成EBOOT.BIN，启动PSPCipher解密。和PARAM.SF0一起用wqsg_umd替换原ISO里的EBOOT.BIN(如果用普罗米修斯模块则是EBOOT.OLD)和PARAM.SF0。在替换时请注意要重新破解。替换后在游戏上按三角看信息就可以看到你的游戏的最新版本。

已经在God Eater 1.01，偶像大师sp补丁上成功。详情参见http://bbs.a9vg.com/read.php?tid=1477236&fpage=1

====================================================================================================================================

PSPCipher by liquidzigong@a9vg.com(aka hrimfaxi)

It can decrypt PRX type 5 (0x2e5e12f0) when prxdecrypter 2.4 etc failed to handle. It's a completely reimplemention version as mesg_led_02g.prx and memlmd_02g.prx from FW 6.20. So if you are clever to find DRM decryption key you can decrypt DRMed module with it.

The sample decrypts host0:/enc/EBOOT.BIN and save to host0:/dec/EBOOT.BIN. kbridge dir contains decryption implemention. Please see pspcipher.h to use the code.

The source is covered by GPLv3 to fight aginst Sony NPDRM.