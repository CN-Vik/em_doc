

# 1. 根据系统启动查看设备树节点文件（系统运行后的）
## 比如：要查看I2C2
```c
	i2c2: i2c@feaa0000 {
		compatible = "rockchip,rk3588-i2c", "rockchip,rk3399-i2c";
		reg = <0x0 0xfeaa0000 0x0 0x1000>;
		clocks = <&cru CLK_I2C2>, <&cru PCLK_I2C2>;
		clock-names = "i2c", "pclk";
		interrupts = <GIC_SPI 319 IRQ_TYPE_LEVEL_HIGH>;
		pinctrl-names = "default";
		pinctrl-0 = <&i2c2m0_xfer>;
		resets = <&cru SRST_I2C2>, <&cru SRST_P_I2C2>;
		reset-names = "i2c", "apb";
		#address-cells = <1>;
		#size-cells = <0>;
		status = "disabled";
	};
```

![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/e508006fb8c9427ca0ce1d96eff24e76.png)

>综上所述，节点名称是i2c@feaa0000

> cd /proc/device-tree/i2c@feaa0000  ,进入这个目录 ls查看
![在这里插入图片描述](https://i-blog.csdnimg.cn/direct/20c0451689254e4790448ffd82733831.png)

# 2. 反编译总的设备树文件来查询

```c
dtc -I dtb -O dts rk3588s-evb1-lp4x-v10-linux.dtb > test_all.dts
```


> 执行这个指令 grep -A10 "i2c2" test_all.dts    
> 结果如下
```c
wls@wls-virtual-machine:~/manifoldtech_file/project/RK3588/rk3588_linux_241112/kernel/arch/arm64/boot/dts/rockchip$ grep -A10 "i2c2" test_all.dts
                i2c2 = "/i2c@feaa0000";
                i2c3 = "/i2c@feab0000";
                i2c4 = "/i2c@feac0000";
                i2c5 = "/i2c@fead0000";
                i2c6 = "/i2c@fec80000";
                i2c7 = "/i2c@fec90000";
                i2c8 = "/i2c@feca0000";
                rkcif_mipi_lvds0 = "/rkcif-mipi-lvds";
                rkcif_mipi_lvds1 = "/rkcif-mipi-lvds1";
                rkcif_mipi_lvds2 = "/rkcif-mipi-lvds2";
                rkcif_mipi_lvds3 = "/rkcif-mipi-lvds3";
--
                i2c2 {

                        i2c2m0-xfer {
                                rockchip,pins = <0x00 0x0f 0x09 0x19d 0x00 0x10 0x09 0x19d>;
                                phandle = <0x143>;
                        };

                        i2c2m2-xfer {
                                rockchip,pins = <0x02 0x03 0x09 0x19d 0x02 0x02 0x09 0x19d>;
                                phandle = <0x341>;
                        };

                        i2c2m3-xfer {
                                rockchip,pins = <0x01 0x15 0x09 0x19d 0x01 0x14 0x09 0x19d>;
                                phandle = <0x342>;
                        };

                        i2c2m4-xfer {
                                rockchip,pins = <0x01 0x01 0x09 0x19d 0x01 0x00 0x09 0x19d>;
                                phandle = <0x343>;
                        };
                };

                i2c3 {

                        i2c3m0-xfer {
                                rockchip,pins = <0x01 0x11 0x09 0x19d 0x01 0x10 0x09 0x19d>;
                                phandle = <0x150>;
--
                i2c2 = "/i2c@feaa0000";
                vdd_npu_s0 = "/i2c@feaa0000/rk8602@42";
                vdd_npu_mem_s0 = "/i2c@feaa0000/rk8602@42";
                aw2013_led = "/i2c@feaa0000/aw2013_led@45";
                imu_icm_45686 = "/i2c@feaa0000/imu_icm_45686@69";
                usbc0 = "/i2c@feaa0000/fusb301@25";
                usbc0_role_sw = "/i2c@feaa0000/fusb301@25/ports/port@0/endpoint@0";
                usb_con = "/i2c@feaa0000/fusb301@25/connector";
                usbc0_orien_sw = "/i2c@feaa0000/fusb301@25/connector/ports/port@0/endpoint";
                dp_altmode_mux = "/i2c@feaa0000/fusb301@25/connector/ports/port@1/endpoint";
                og02b10_1 = "/i2c@feaa0000/og02b10-1@70";
--
                i2c2m0_xfer = "/pinctrl/i2c2/i2c2m0-xfer";
                i2c2m2_xfer = "/pinctrl/i2c2/i2c2m2-xfer";
                i2c2m3_xfer = "/pinctrl/i2c2/i2c2m3-xfer";
                i2c2m4_xfer = "/pinctrl/i2c2/i2c2m4-xfer";
                i2c3m0_xfer = "/pinctrl/i2c3/i2c3m0-xfer";
                i2c3m1_xfer = "/pinctrl/i2c3/i2c3m1-xfer";
                i2c3m2_xfer = "/pinctrl/i2c3/i2c3m2-xfer";
                i2c3m4_xfer = "/pinctrl/i2c3/i2c3m4-xfer";
                i2c4m0_xfer = "/pinctrl/i2c4/i2c4m0-xfer";
                i2c4m2_xfer = "/pinctrl/i2c4/i2c4m2-xfer";
                i2c4m3_xfer = "/pinctrl/i2c4/i2c4m3-xfer";
                i2c4m4_xfer = "/pinctrl/i2c4/i2c4m4-xfer";
                i2c5m0_xfer = "/pinctrl/i2c5/i2c5m0-xfer";
                i2c5m1_xfer = "/pinctrl/i2c5/i2c5m1-xfer";
```
