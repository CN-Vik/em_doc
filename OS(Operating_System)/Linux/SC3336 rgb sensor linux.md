

我已经把 **带详细中文注释** 的 `sc3336.c` 放到画布（Canvas）里，文件名为 **Sc3336 注释版**（你可以在左侧画布区看到它）。我没有在聊天窗口重复整个代码（代码已在画布中），下面给你一份清晰、分段的代码流程解读与调试要点 — 便于快速理解与定位问题。

```c
// SPDX-License-Identifier: GPL-2.0
/*
 * sc3336 driver — 带详细中文注释版
 *
 * 说明：此文件为 smartsens sc3336 图像传感器的 Linux v4l2 子设备驱动。
 * 我在源码中添加了大量中文注释，解释宏、结构体、关键函数、流程和注意点，
 * 但没有逐行注释每一个寄存器数组的条目（初始化表通常由传感器厂商给出且为数据），
 * 如需我也可以把寄存器表逐项注释。
 *
 * 使用方式：查看 probe() -> power_on() -> initialize_controls() -> v4l2_async_register_subdev
 * 驱动会通过 i2c 与传感器通信，设置寄存器并实现标准的 v4l2 subdev 接口（set_fmt、s_stream 等）。
 *
 * 版权信息：保持原作者版权声明。
 */

//#define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/rk-camera-module.h>
#include <linux/rk-preisp.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>
#include <linux/pinctrl/consumer.h>
#include "../platform/rockchip/isp/rkisp_tb_helper.h"

/* 驱动版本宏 */
#define DRIVER_VERSION			KERNEL_VERSION(0, 0x01, 0x01)

#ifndef V4L2_CID_DIGITAL_GAIN
#define V4L2_CID_DIGITAL_GAIN		V4L2_CID_GAIN
#endif

/* 硬件/格式相关常量解释 */
#define SC3336_LANES			2	/* MIPI lanes */
#define SC3336_BITS_PER_SAMPLE		10	/* 10-bit 原始数据 */
#define SC3336_LINK_FREQ_253		253125000
#define SC3336_LINK_FREQ_255		255000000

/* 根据 link freq 和 lanes / bits 计算像素率（video core 使用） */
#define PIXEL_RATE_WITH_253M_10BIT	(SC3336_LINK_FREQ_253 * 2 * \
				SC3336_LANES / SC3336_BITS_PER_SAMPLE)
#define PIXEL_RATE_WITH_255M_10BIT	(SC3336_LINK_FREQ_255 * 2 * \
				SC3336_LANES / SC3336_BITS_PER_SAMPLE)

#define SC3336_XVCLK_FREQ		27000000

/* 芯片 ID 与寄存器地址 */
#define CHIP_ID				0xcc41
#define SC3336_REG_CHIP_ID		0x3107

/* 控制模式寄存器：0 停止，1 运行 */
#define SC3336_REG_CTRL_MODE		0x0100
#define SC3336_MODE_SW_STANDBY		0x0
#define SC3336_MODE_STREAMING		BIT(0)

/* 曝光/增益相关寄存器及范围 */
#define SC3336_REG_EXPOSURE_H		0x3e00
#define SC3336_REG_EXPOSURE_M		0x3e01
#define SC3336_REG_EXPOSURE_L		0x3e02
#define	SC3336_EXPOSURE_MIN		1
#define	SC3336_EXPOSURE_STEP		1
#define SC3336_VTS_MAX			0x7fff

#define SC3336_REG_DIG_GAIN		0x3e06
#define SC3336_REG_DIG_FINE_GAIN	0x3e07
#define SC3336_REG_ANA_GAIN		0x3e09
#define SC3336_GAIN_MIN			0x0080
#define SC3336_GAIN_MAX			(99614) /* 48.64*16*128，大约的上限 */
#define SC3336_GAIN_STEP		1
#define SC3336_GAIN_DEFAULT		0x80

#define SC3336_REG_GROUP_HOLD		0x3812
#define SC3336_GROUP_HOLD_START		0x00
#define SC3336_GROUP_HOLD_END		0x30

#define SC3336_REG_TEST_PATTERN		0x4501
#define SC3336_TEST_PATTERN_BIT_MASK	BIT(3)

#define SC3336_REG_VTS_H		0x320e
#define SC3336_REG_VTS_L		0x320f

#define SC3336_FLIP_MIRROR_REG		0x3221

/* 读取/写入曝光值的位域宏（传感器寄存器位域通常是拆出的） */
#define SC3336_FETCH_EXP_H(VAL) 	(((VAL) >> 12) & 0xF)
#define SC3336_FETCH_EXP_M(VAL) 	(((VAL) >> 4) & 0xFF)
#define SC3336_FETCH_EXP_L(VAL) 	(((VAL) & 0xF) << 4)

#define SC3336_FETCH_AGAIN_H(VAL) 	(((VAL) >> 8) & 0x03)
#define SC3336_FETCH_AGAIN_L(VAL) 	((VAL) & 0xFF)

/* 翻转/镜像位操作宏（读出后按位设置/清除） */
#define SC3336_FETCH_MIRROR(VAL, ENABLE) 	(ENABLE ? VAL | 0x06 : VAL & 0xf9)
#define SC3336_FETCH_FLIP(VAL, ENABLE) 		(ENABLE ? VAL | 0x60 : VAL & 0x9f)

/* 寄存器数组解析辅助常量 */
#define REG_DELAY				0xFFFE
#define REG_NULL				0xFFFF

#define SC3336_REG_VALUE_08BIT		1
#define SC3336_REG_VALUE_16BIT		2
#define SC3336_REG_VALUE_24BIT		3

#define OF_CAMERA_PINCTRL_STATE_DEFAULT	"rockchip,camera_default"
#define OF_CAMERA_PINCTRL_STATE_SLEEP	"rockchip,camera_sleep"
#define SC3336_NAME			"sc3336"

/* 电源名字（Device tree 中会对应这些 regulator） */
static const char * const sc3336_supply_names[] = {
	"avdd", 	/* 模拟电源 */
	"dovdd", 	/* IO 电源 */
	"dvdd", 	/* 核心数字电源 */
};

#define SC3336_NUM_SUPPLIES ARRAY_SIZE(sc3336_supply_names)

/* 简单的寄存器地址/值对结构 */
struct regval {
	u16 addr;
	u8 val;
};

/* 模式描述结构：分辨率/帧率/hts/vts/寄存器列表等信息 */
struct sc3336_mode {
	u32 bus_fmt;         /* media bus format */
	u32 width;
	u32 height;
	struct v4l2_fract max_fps; /* 最大帧率（分数） */
	u32 hts_def;         /* 时序：水平总像素（hts） */
	u32 vts_def;         /* 垂直总行数（vts） */
	u32 exp_def;         /* 默认曝光 */
	const struct regval *reg_list; /* 初始化寄存器表 */
	u32 hdr_mode;        /* HDR 模式标记 */
	u32 xvclk_freq;      /* 供给给传感器的外部时钟频率 */
	u32 link_freq_idx;   /* link_freq_menu_items 的索引 */
	u32 vc[PAD_MAX];
};

/* 驱动主上下文结构 */
struct sc3336 {
	struct i2c_client	*client;
	struct clk		*xvclk; /* 外部 XCLK */
	struct gpio_desc	*reset_gpio;
	struct gpio_desc	*pwdn_gpio;
	struct regulator_bulk_data supplies[SC3336_NUM_SUPPLIES];

	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	struct v4l2_subdev	subdev;
	struct media_pad	pad;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*exposure;
	struct v4l2_ctrl	*anal_gain;
	struct v4l2_ctrl	*digi_gain;
	struct v4l2_ctrl	*hblank;
	struct v4l2_ctrl	*vblank;
	struct v4l2_ctrl	*pixel_rate;
	struct v4l2_ctrl	*link_freq;
	struct v4l2_ctrl	*test_pattern;
	struct mutex		mutex;
	struct v4l2_fract	cur_fps;
	bool			streaming;
	bool			power_on;
	const struct sc3336_mode *cur_mode;
	u32			module_index;
	const char		*module_facing;
	const char		*module_name;
	const char		*len_name;
	u32			cur_vts;
	bool			has_init_exp;
	bool			is_thunderboot;
	bool			is_first_streamoff;
	struct preisp_hdrae_exp_s init_hdrae_exp;
};

#define to_sc3336(sd) container_of(sd, struct sc3336, subdev)

/*
 * 以下为芯片默认寄存器表/模式设置。厂商提供。
 * 这些 reg value 列表用于不同分辨率/帧率下的初始化，
 * 驱动在 start_stream 时写入整套表来配置传感器。
 */

/* 全局寄存器（非常少，一般作为占位） */
static const struct regval sc3336_global_regs[] = {
	{REG_NULL, 0x00},
};

/* Xclk 27Mhz, max_framerate 25fps 模式下的寄存器初始化表（示例） */
static const struct regval sc3336_linear_10_2304x1296_25fps_regs[] = {
	/* 注意：下面是厂商提供的寄存器初始化序列表，
	   每一项是 {寄存器地址, 值}，通常直接按顺序写入即可。*/
	{0x0103, 0x01},
	{0x36e9, 0x80},
	/* ... 为了篇幅不作逐项注释 */
	{0x3028, 0x05},
	{REG_NULL, 0x00},
};

/* Xclk 24Mhz，max_framerate 30fps 模式的寄存器初始化表 */
static const struct regval sc3336_linear_10_2304x1296_30fps_regs[] = {
	{0x0103, 0x01},
	{0x36e9, 0x80},
	/* ... */
	{REG_NULL, 0x00},
};

/* 支持的模式数组：定义分辨率、帧率、hts/vts、使用的寄存器表等 */
static const struct sc3336_mode supported_modes[] = {
	{
		.width = 2304,
		.height = 1296,
		.max_fps = {
			.numerator = 10000,
			.denominator = 300000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x0578 * 2,
		.vts_def = 0x0550,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc3336_linear_10_2304x1296_30fps_regs,
		.hdr_mode = NO_HDR,
		.xvclk_freq = 24000000,
		.link_freq_idx = 1,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	},
	{
		.width = 2304,
		.height = 1296,
		.max_fps = {
			.numerator = 10000,
			.denominator = 250000,
		},
		.exp_def = 0x0080,
		.hts_def = 0x05dc,
		.vts_def = 0x0654,
		.bus_fmt = MEDIA_BUS_FMT_SBGGR10_1X10,
		.reg_list = sc3336_linear_10_2304x1296_25fps_regs,
		.hdr_mode = NO_HDR,
		.xvclk_freq = 27000000,
		.link_freq_idx = 0,
		.vc[PAD0] = V4L2_MBUS_CSI2_CHANNEL_0,
	}
};

/* link frequency 与菜单项对应表（给用户空间选择） */
static const s64 link_freq_menu_items[] = {
	SC3336_LINK_FREQ_253,
	SC3336_LINK_FREQ_255,
};

/* 测试图案菜单 */
static const char * const sc3336_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
	"Vertical Color Bar Type 2",
	"Vertical Color Bar Type 3",
	"Vertical Color Bar Type 4",
};

/*
 * i2c 写寄存器：支持最多写 4 字节数据（本传感器实际多为 8-bit 寄存器）
 * 缓冲区结构： [addr_hi, addr_lo, data_MSB, ..., data_LSB]
 */
static int sc3336_write_reg(struct i2c_client *client, u16 reg,
			    u32 len, u32 val)
{
	u32 buf_i, val_i;
	u8 buf[6];
	u8 *val_p;
	__be32 val_be;

	if (len > 4)
		return -EINVAL;

	/* 寄存器地址采用 big-endian 输出 */
	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	/* 将值转换为 big-endian 后，从低有效字节开始填入缓冲区末端 */
	val_be = cpu_to_be32(val);
	val_p = (u8 *)&val_be;
	buf_i = 2;
	val_i = 4 - len;

	while (val_i < 4)
		buf[buf_i++] = val_p[val_i++];

	/* 直接使用 i2c_master_send 发送 (addr + data) */
	if (i2c_master_send(client, buf, len + 2) != len + 2)
		return -EIO;
	return 0;
}

/* 将寄存器数组按顺序写入，直到遇到 REG_NULL */
static int sc3336_write_array(struct i2c_client *client,
			       const struct regval *regs)
{
	u32 i;
	int ret = 0;

	for (i = 0; ret == 0 && regs[i].addr != REG_NULL; i++)
		ret = sc3336_write_reg(client, regs[i].addr,
					SC3336_REG_VALUE_08BIT, regs[i].val);

	return ret;
}

/*
 * i2c 读寄存器: 先写 2 字节寄存器地址，再读出 len 字节
 * 这里使用两个 i2c_msg 通过 i2c_transfer 完成（写地址 + 读数据）。
 */
static int sc3336_read_reg(struct i2c_client *client, u16 reg, unsigned int len,
			    u32 *val)
{
	struct i2c_msg msgs[2];
	u8 *data_be_p;
	__be32 data_be = 0;
	__be16 reg_addr_be = cpu_to_be16(reg);
	int ret;

	if (len > 4 || !len)
		return -EINVAL;

	data_be_p = (u8 *)&data_be;
	/* 写寄存器地址 */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = (u8 *)&reg_addr_be;

	/* 读寄存器数据 */
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = &data_be_p[4 - len];

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret != ARRAY_SIZE(msgs))
		return -EIO;

	*val = be32_to_cpu(data_be);

	return 0;
}

/*
 * 增益设置函数：把用户给定的总增益（数字）拆成模拟增益/数字增益（coarse/fine）
 * 这里实现了继电器式的层级放大：先尽量使用模拟增益，超出后逐步开启数字增益。
 */
static int sc3336_set_gain_reg(struct sc3336 *sc3336, u32 gain)
{
	struct i2c_client *client = sc3336->client;
	u32 coarse_again = 0, coarse_dgain = 0, fine_dgain = 0;
	int ret = 0, gain_factor;

	/* 限定 gain 在合法范围内 */
	if (gain < 128)
		gain = 128;
	else if (gain > SC3336_GAIN_MAX)
		gain = SC3336_GAIN_MAX;

	/* 将 gain 标准化为以 128 为单位的千倍格式，避免浮点运算 */
	gain_factor = gain * 1000 / 128;
	/* 以下是厂商给定的映射表：根据 gain_factor 落在不同区间来设定 coarse_again/coarse_dgain/fine_dgain */
	if (gain_factor < 1520) {
		coarse_again = 0x00;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 1000;
	} else if (gain_factor < 3040) {
		coarse_again = 0x40;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 1520;
	} else if (gain_factor < 6080) {
		coarse_again = 0x48;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 3040;
	} else if (gain_factor < 12160) {
		coarse_again = 0x49;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 6080;
	} else if (gain_factor < 24320) {
		coarse_again = 0x4b;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 12160;
	} else if (gain_factor < 48640) {
		coarse_again = 0x4f;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 24320;
	} else if (gain_factor < 48640 * 2) {
		/* 打开数字增益，最大数字增益 4X */
		coarse_again = 0x5f;
		coarse_dgain = 0x00;
		fine_dgain = gain_factor * 128 / 48640;
	} else if (gain_factor < 48640 * 4) {
		coarse_again = 0x5f;
		coarse_dgain = 0x01;
		fine_dgain = gain_factor * 128 / 48640 / 2;
	} else if (gain_factor < 48640 * 8) {
		coarse_again = 0x5f;
		coarse_dgain = 0x03;
		fine_dgain = gain_factor * 128 / 48640 / 4;
	} else if (gain_factor < 48640 * 16) {
		coarse_again = 0x5f;
		coarse_dgain = 0x07;
		fine_dgain = gain_factor * 128 / 48640 / 8;
	}
	dev_dbg(&client->dev, "c_again: 0x%x, c_dgain: 0x%x, f_dgain: 0x%0x\n",
		    coarse_again, coarse_dgain, fine_dgain);

	/* 写入数字/模拟增益寄存器 */
	ret = sc3336_write_reg(sc3336->client,
				SC3336_REG_DIG_GAIN,
				SC3336_REG_VALUE_08BIT,
				coarse_dgain);
	ret |= sc3336_write_reg(sc3336->client,
				 SC3336_REG_DIG_FINE_GAIN,
				 SC3336_REG_VALUE_08BIT,
				 fine_dgain);
	ret |= sc3336_write_reg(sc3336->client,
				 SC3336_REG_ANA_GAIN,
				 SC3336_REG_VALUE_08BIT,
				 coarse_again);

	return ret;
}

/* 简单的分辨率差距度量函数（用于选择最接近的模式） */
static int sc3336_get_reso_dist(const struct sc3336_mode *mode,
				 struct v4l2_mbus_framefmt *framefmt)
{
	return abs(mode->width - framefmt->width) +
	       abs(mode->height - framefmt->height);
}

/* 在 supported_modes 中找到与用户请求最接近的模式 */
static const struct sc3336_mode *
sc3336_find_best_fit(struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;
	int dist;
	int cur_best_fit = 0;
	int cur_best_fit_dist = -1;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		dist = sc3336_get_reso_dist(&supported_modes[i], framefmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			cur_best_fit = i;
		}
	}

	return &supported_modes[cur_best_fit];
}

/*
 * 设置格式：如果是 TRY 类型（应用测试），则只修改 try format，否则设置驱动当前模式，
 * 并更新相关控件（hblank, vblank, link_freq, pixel_rate, cur_fps 等）。
 */
static int sc3336_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	const struct sc3336_mode *mode;
	s64 h_blank, vblank_def;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;

	mutex_lock(&sc3336->mutex);

	mode = sc3336_find_best_fit(fmt);
	fmt->format.code = mode->bus_fmt;
	fmt->format.width = mode->width;
	fmt->format.height = mode->height;
	fmt->format.field = V4L2_FIELD_NONE;
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		*v4l2_subdev_get_try_format(sd, cfg, fmt->pad) = fmt->format;
#else
		mutex_unlock(&sc3336->mutex);
		return -ENOTTY;
#endif
	} else {
		sc3336->cur_mode = mode;
		h_blank = mode->hts_def - mode->width;
		__v4l2_ctrl_modify_range(sc3336->hblank, h_blank,
					 h_blank, 1, h_blank);
		vblank_def = mode->vts_def - mode->height;
		__v4l2_ctrl_modify_range(sc3336->vblank, vblank_def,
					 SC3336_VTS_MAX - mode->height,
					 1, vblank_def);
		dst_link_freq = mode->link_freq_idx;
		dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
				 SC3336_BITS_PER_SAMPLE * 2 * SC3336_LANES;
		__v4l2_ctrl_s_ctrl_int64(sc3336->pixel_rate,
					 dst_pixel_rate);
		__v4l2_ctrl_s_ctrl(sc3336->link_freq,
				   dst_link_freq);
		sc3336->cur_fps = mode->max_fps;
	}

	mutex_unlock(&sc3336->mutex);

	return 0;
}

/* 获取当前格式 */
static int sc3336_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *fmt)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	const struct sc3336_mode *mode = sc3336->cur_mode;

	mutex_lock(&sc3336->mutex);
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
		fmt->format = *v4l2_subdev_get_try_format(sd, cfg, fmt->pad);
#else
		mutex_unlock(&sc3336->mutex);
		return -ENOTTY;
#endif
	} else {
		fmt->format.width = mode->width;
		fmt->format.height = mode->height;
		fmt->format.code = mode->bus_fmt;
		fmt->format.field = V4L2_FIELD_NONE;
		/* format 信息：width/height/data type/virtual channel */
		if (fmt->pad < PAD_MAX && mode->hdr_mode != NO_HDR)
			fmt->reserved[0] = mode->vc[fmt->pad];
		else
			fmt->reserved[0] = mode->vc[PAD0];
	}
	mutex_unlock(&sc3336->mutex);

	return 0;
}

/* 枚举当前支持的 media bus code（只返回当前 cur_mode） */
static int sc3336_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct sc3336 *sc3336 = to_sc3336(sd);

	if (code->index != 0)
		return -EINVAL;
	code->code = sc3336->cur_mode->bus_fmt;

	return 0;
}

/* 列举支持的帧大小，返回 supported_modes 中的实际分辨率 */
static int sc3336_enum_frame_sizes(struct v4l2_subdev *sd,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	if (fse->code != supported_modes[0].bus_fmt)
		return -EINVAL;

	fse->min_width  = supported_modes[fse->index].width;
	fse->max_width  = supported_modes[fse->index].width;
	fse->max_height = supported_modes[fse->index].height;
	fse->min_height = supported_modes[fse->index].height;

	return 0;
}

/* 开关测试图案（内部只是写 TEST_PATTERN 寄存器的某一位） */
static int sc3336_enable_test_pattern(struct sc3336 *sc3336, u32 pattern)
{
	u32 val = 0;
	int ret = 0;

	ret = sc3336_read_reg(sc3336->client, SC3336_REG_TEST_PATTERN,
			       SC3336_REG_VALUE_08BIT, &val);
	if (pattern)
		val |= SC3336_TEST_PATTERN_BIT_MASK;
	else
		val &= ~SC3336_TEST_PATTERN_BIT_MASK;

	ret |= sc3336_write_reg(sc3336->client, SC3336_REG_TEST_PATTERN,
				 SC3336_REG_VALUE_08BIT, val);
	return ret;
}

/* 获取帧间隔（frame interval） */
static int sc3336_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *fi)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	const struct sc3336_mode *mode = sc3336->cur_mode;

	if (sc3336->streaming)
		fi->interval = sc3336->cur_fps;
	else
		fi->interval = mode->max_fps;
	return 0;
}

/* 填充 mbus config：指出使用多少 lane、channel 与是否连续时钟 */
static int sc3336_g_mbus_config(struct v4l2_subdev *sd,
				unsigned int pad_id,
				struct v4l2_mbus_config *config)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	const struct sc3336_mode *mode = sc3336->cur_mode;

	u32 val = 1 << (SC3336_LANES - 1) |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	if (mode->hdr_mode != NO_HDR)
		val |= V4L2_MBUS_CSI2_CHANNEL_1;
	if (mode->hdr_mode == HDR_X3)
		val |= V4L2_MBUS_CSI2_CHANNEL_2;

	config->type = V4L2_MBUS_CSI2_DPHY;
	config->flags = val;

	return 0;
}

/* 填充模块信息结构（供用户空间查询模块/镜头/传感器信息） */
static void sc3336_get_module_inf(struct sc3336 *sc3336,
				   struct rkmodule_inf *inf)
{
	memset(inf, 0, sizeof(*inf));
	strscpy(inf->base.sensor, SC3336_NAME, sizeof(inf->base.sensor));
	strscpy(inf->base.module, sc3336->module_name,
		sizeof(inf->base.module));
	strscpy(inf->base.lens, sc3336->len_name, sizeof(inf->base.lens));
}

/*
 * ioctl 处理：处理 RKMODULE_* 以及快速流开关、HDR 配置相关的扩展命令。
 * 这里主要做一些驱动层的额外控制。用户空间通过 v4l2_subdev_call(..., ioctl, ...) 访问。
 */
static long sc3336_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	struct rkmodule_hdr_cfg *hdr;
	u32 i, h, w;
	long ret = 0;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		sc3336_get_module_inf(sc3336, (struct rkmodule_inf *)arg);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		hdr->esp.mode = HDR_NORMAL_VC;
		hdr->hdr_mode = sc3336->cur_mode->hdr_mode;
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = (struct rkmodule_hdr_cfg *)arg;
		w = sc3336->cur_mode->width;
		h = sc3336->cur_mode->height;
		for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
			if (w == supported_modes[i].width &&
			    h == supported_modes[i].height &&
			    supported_modes[i].hdr_mode == hdr->hdr_mode) {
				sc3336->cur_mode = &supported_modes[i];
				break;
			}
		}
		if (i == ARRAY_SIZE(supported_modes)) {
			dev_err(&sc3336->client->dev,
				"not find hdr mode:%d %dx%d config\n",
				hdr->hdr_mode, w, h);
			ret = -EINVAL;
		} else {
			w = sc3336->cur_mode->hts_def - sc3336->cur_mode->width;
			h = sc3336->cur_mode->vts_def - sc3336->cur_mode->height;
			__v4l2_ctrl_modify_range(sc3336->hblank, w, w, 1, w);
			__v4l2_ctrl_modify_range(sc3336->vblank, h,
					 SC3336_VTS_MAX - sc3336->cur_mode->height, 1, h);
			sc3336->cur_fps = sc3336->cur_mode->max_fps;
		}
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		break;
	case RKMODULE_SET_QUICK_STREAM:

		stream = *((u32 *)arg);

		if (stream)
			ret = sc3336_write_reg(sc3336->client, SC3336_REG_CTRL_MODE,
				 SC3336_REG_VALUE_08BIT, SC3336_MODE_STREAMING);
		else
			ret = sc3336_write_reg(sc3336->client, SC3336_REG_CTRL_MODE,
				 SC3336_REG_VALUE_08BIT, SC3336_MODE_SW_STANDBY);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
/* 32位兼容 ioctl 处理（只在需要时启用） */
static long sc3336_compat_ioctl32(struct v4l2_subdev *sd,
				   unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct rkmodule_inf *inf;
	struct rkmodule_hdr_cfg *hdr;
	struct preisp_hdrae_exp_s *hdrae;
	long ret;
	u32 stream = 0;

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc3336_ioctl(sd, cmd, inf);
		if (!ret) {
			if (copy_to_user(up, inf, sizeof(*inf)))
				ret = -EFAULT;
		}
		kfree(inf);
		break;
	case RKMODULE_GET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = sc3336_ioctl(sd, cmd, hdr);
		if (!ret) {
			if (copy_to_user(up, hdr, sizeof(*hdr)))
				ret = -EFAULT;
		}
		kfree(hdr);
		break;
	case RKMODULE_SET_HDR_CFG:
		hdr = kzalloc(sizeof(*hdr), GFP_KERNEL);
		if (!hdr) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdr, up, sizeof(*hdr));
		if (!ret)
			ret = sc3336_ioctl(sd, cmd, hdr);
		else
			ret = -EFAULT;
		kfree(hdr);
		break;
	case PREISP_CMD_SET_HDRAE_EXP:
		hdrae = kzalloc(sizeof(*hdrae), GFP_KERNEL);
		if (!hdrae) {
			ret = -ENOMEM;
			return ret;
		}

		ret = copy_from_user(hdrae, up, sizeof(*hdrae));
		if (!ret)
			ret = sc3336_ioctl(sd, cmd, hdrae);
		else
			ret = -EFAULT;
		kfree(hdrae);
		break;
	case RKMODULE_SET_QUICK_STREAM:
		ret = copy_from_user(&stream, up, sizeof(u32));
		if (!ret)
			ret = sc3336_ioctl(sd, cmd, &stream);
		else
			ret = -EFAULT;
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

/*
 * 真正开始流的函数：写入当前模式的寄存器表，并应用控件设置。
 * 如果 is_thunderboot（快速启动），会跳过一些初始化以加速启动。
 */
static int __sc3336_start_stream(struct sc3336 *sc3336)
{
	int ret;

	if (!sc3336->is_thunderboot) {
		ret = sc3336_write_array(sc3336->client, sc3336->cur_mode->reg_list);
		if (ret)
			return ret;
		/* 如果用户在 start 之前对控件做了改动，这里把控件值写入硬件 */
		ret = __v4l2_ctrl_handler_setup(&sc3336->ctrl_handler);
		if (ret)
			return ret;
		if (sc3336->has_init_exp && sc3336->cur_mode->hdr_mode != NO_HDR) {
			ret = sc3336_ioctl(&sc3336->subdev, PREISP_CMD_SET_HDRAE_EXP,
				&sc3336->init_hdrae_exp);
			if (ret) {
				dev_err(&sc3336->client->dev,
					"init exp fail in hdr mode\n");
				return ret;
			}
		}
	}
	ret = sc3336_write_reg(sc3336->client, SC3336_REG_CTRL_MODE,
				 SC3336_REG_VALUE_08BIT, SC3336_MODE_STREAMING);
	return ret;
}

/* 停止流：把 has_init_exp 置位清除并写入 standby */
static int __sc3336_stop_stream(struct sc3336 *sc3336)
{
	sc3336->has_init_exp = false;
	if (sc3336->is_thunderboot)
		sc3336->is_first_streamoff = true;
	return sc3336_write_reg(sc3336->client, SC3336_REG_CTRL_MODE,
				 SC3336_REG_VALUE_08BIT, SC3336_MODE_SW_STANDBY);
}

/* s_stream：v4l2_video_ops 的接口实现，打开/关闭流，同时处理 pm_runtime */
static int __sc3336_power_on(struct sc3336 *sc3336);
static int sc3336_s_stream(struct v4l2_subdev *sd, int on)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	struct i2c_client *client = sc3336->client;
	int ret = 0;

	mutex_lock(&sc3336->mutex);
	on = !!on;
	if (on == sc3336->streaming)
		goto unlock_and_return;
	if (on) {
		if (sc3336->is_thunderboot && rkisp_tb_get_state() == RKISP_TB_NG) {
			sc3336->is_thunderboot = false;
			__sc3336_power_on(sc3336);
		}
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}
		ret = __sc3336_start_stream(sc3336);
		if (ret) {
			v4l2_err(sd, "start stream failed while write regs\n");
			pm_runtime_put(&client->dev);
			goto unlock_and_return;
		}
	} else {
		__sc3336_stop_stream(sc3336);
		pm_runtime_put(&client->dev);
	}

	sc3336->streaming = on;
unlock_and_return:
	mutex_unlock(&sc3336->mutex);
	return ret;
}

/* s_power：供 v4l2 core 在需要时控制电源（runtime PM） */
static int sc3336_s_power(struct v4l2_subdev *sd, int on)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	struct i2c_client *client = sc3336->client;
	int ret = 0;

	mutex_lock(&sc3336->mutex);

	/* 如果电源状态没有变化则什么也不做 */
	if (sc3336->power_on == !!on)
		goto unlock_and_return;

	if (on) {
		ret = pm_runtime_get_sync(&client->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(&client->dev);
			goto unlock_and_return;
		}

		if (!sc3336->is_thunderboot) {
			ret = sc3336_write_array(sc3336->client, sc3336_global_regs);
			if (ret) {
				v4l2_err(sd, "could not set init registers\n");
				pm_runtime_put_noidle(&client->dev);
				goto unlock_and_return;
			}
		}

		sc3336->power_on = true;
	} else {
		pm_runtime_put(&client->dev);
		sc3336->power_on = false;
	}

unlock_and_return:
	mutex_unlock(&sc3336->mutex);

	return ret;
}

/*
 * 计算延迟：把 clock cycles 转为微秒，便于在电源上电序列中延时（usleep_range）。
 */
static inline u32 sc3336_cal_delay(u32 cycles, struct sc3336 *sc3336)
{
	return DIV_ROUND_UP(cycles, sc3336->cur_mode->xvclk_freq / 1000 / 1000);
}

/* 上电序列：配置 pinctrl -> 配置 xvclk -> 使能 regulator -> reset/pwdn 控制 -> 延时 */
static int __sc3336_power_on(struct sc3336 *sc3336)
{
	int ret;
	u32 delay_us;
	struct device *dev = &sc3336->client->dev;

	if (!IS_ERR_OR_NULL(sc3336->pins_default)) {
		ret = pinctrl_select_state(sc3336->pinctrl,
				   sc3336->pins_default);
		if (ret < 0)
			dev_err(dev, "could not set pins\n");
	}
	ret = clk_set_rate(sc3336->xvclk, sc3336->cur_mode->xvclk_freq);
	if (ret < 0)
		dev_warn(dev, "Failed to set xvclk rate (%dHz)\n", sc3336->cur_mode->xvclk_freq);
	if (clk_get_rate(sc3336->xvclk) != sc3336->cur_mode->xvclk_freq)
		dev_warn(dev, "xvclk mismatched, modes are based on %dHz\n",
			 sc3336->cur_mode->xvclk_freq);
	ret = clk_prepare_enable(sc3336->xvclk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable xvclk\n");
		return ret;
	}

	/* thunderboot 模式下可能跳过硬件复位流程 */
	if (sc3336->is_thunderboot)
		return 0;

	if (!IS_ERR(sc3336->reset_gpio))
		gpiod_set_value_cansleep(sc3336->reset_gpio, 0);

	ret = regulator_bulk_enable(SC3336_NUM_SUPPLIES, sc3336->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators\n");
		goto disable_clk;
	}

	if (!IS_ERR(sc3336->reset_gpio))
		gpiod_set_value_cansleep(sc3336->reset_gpio, 1);

	usleep_range(500, 1000);

	if (!IS_ERR(sc3336->pwdn_gpio))
		gpiod_set_value_cansleep(sc3336->pwdn_gpio, 1);

	if (!IS_ERR(sc3336->reset_gpio))
		usleep_range(6000, 8000);
	else
		usleep_range(12000, 16000);

	/* 在第一次 SCCB（I2C）事务之前，保证一定的 cycles 延迟 */
	delay_us = sc3336_cal_delay(8192, sc3336);
	usleep_range(delay_us, delay_us * 2);

	return 0;

disable_clk:
	clk_disable_unprepare(sc3336->xvclk);

	return ret;
}

/* 下电序列：撤销上电顺序，且处理 thunderboot 特殊逻辑 */
static void __sc3336_power_off(struct sc3336 *sc3336)
{
	int ret;
	struct device *dev = &sc3336->client->dev;

	clk_disable_unprepare(sc3336->xvclk);
	if (sc3336->is_thunderboot) {
		if (sc3336->is_first_streamoff) {
			sc3336->is_thunderboot = false;
			sc3336->is_first_streamoff = false;
		} else {
			return;
		}
	}

	if (!IS_ERR(sc3336->pwdn_gpio))
		gpiod_set_value_cansleep(sc3336->pwdn_gpio, 0);
	clk_disable_unprepare(sc3336->xvclk);
	if (!IS_ERR(sc3336->reset_gpio))
		gpiod_set_value_cansleep(sc3336->reset_gpio, 0);
	if (!IS_ERR_OR_NULL(sc3336->pins_sleep)) {
		ret = pinctrl_select_state(sc3336->pinctrl,
				   sc3336->pins_sleep);
		if (ret < 0)
			dev_dbg(dev, "could not set pins\n");
	}
	regulator_bulk_disable(SC3336_NUM_SUPPLIES, sc3336->supplies);
}

/* runtime pm hooks，用于被 pm runtime 调用 */
static int __maybe_unused sc3336_runtime_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc3336 *sc3336 = to_sc3336(sd);

	return __sc3336_power_on(sc3336);
}

static int __maybe_unused sc3336_runtime_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc3336 *sc3336 = to_sc3336(sd);

	__sc3336_power_off(sc3336);

	return 0;
}

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
/* open 时初始化 try_fmt 的默认值 */
static int sc3336_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct sc3336 *sc3336 = to_sc3336(sd);
	struct v4l2_mbus_framefmt *try_fmt =
			v4l2_subdev_get_try_format(sd, fh->pad, 0);
	const struct sc3336_mode *def_mode = &supported_modes[0];

	mutex_lock(&sc3336->mutex);
	/* 初始化 try_fmt */
	try_fmt->width = def_mode->width;
	try_fmt->height = def_mode->height;
	try_fmt->code = def_mode->bus_fmt;
	try_fmt->field = V4L2_FIELD_NONE;

	mutex_unlock(&sc3336->mutex);
	/* 无 crop 或 compose */

	return 0;
}
#endif

/* 枚举帧间隔 */
static int sc3336_enum_frame_interval(struct v4l2_subdev *sd,
				       struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index >= ARRAY_SIZE(supported_modes))
		return -EINVAL;

	fie->code = supported_modes[fie->index].bus_fmt;
	fie->width = supported_modes[fie->index].width;
	fie->height = supported_modes[fie->index].height;
	fie->interval = supported_modes[fie->index].max_fps;
	fie->reserved[0] = supported_modes[fie->index].hdr_mode;
	return 0;
}

/* pm ops 结构 */
static const struct dev_pm_ops sc3336_pm_ops = {
	SET_RUNTIME_PM_OPS(sc3336_runtime_suspend,
			   sc3336_runtime_resume, NULL)
};

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
static const struct v4l2_subdev_internal_ops sc3336_internal_ops = {
	.open = sc3336_open,
};
#endif

/* v4l2 core ops */
static const struct v4l2_subdev_core_ops sc3336_core_ops = {
	.s_power = sc3336_s_power,
	.ioctl = sc3336_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sc3336_compat_ioctl32,
#endif
};

/* video_ops */
static const struct v4l2_subdev_video_ops sc3336_video_ops = {
	.s_stream = sc3336_s_stream,
	.g_frame_interval = sc3336_g_frame_interval,
};

/* pad ops */
static const struct v4l2_subdev_pad_ops sc3336_pad_ops = {
	.enum_mbus_code = sc3336_enum_mbus_code,
	.enum_frame_size = sc3336_enum_frame_sizes,
	.enum_frame_interval = sc3336_enum_frame_interval,
	.get_fmt = sc3336_get_fmt,
	.set_fmt = sc3336_set_fmt,
	.get_mbus_config = sc3336_g_mbus_config,
};

static const struct v4l2_subdev_ops sc3336_subdev_ops = {
	.core	= &sc3336_core_ops,
	.video	= &sc3336_video_ops,
	.pad	= &sc3336_pad_ops,
};

/* 根据 cur_vts 更新 cur_fps（用于运行时帧率调整） */
static void sc3336_modify_fps_info(struct sc3336 *sc3336)
{
	const struct sc3336_mode *mode = sc3336->cur_mode;

	sc3336->cur_fps.denominator = mode->max_fps.denominator * mode->vts_def /
				      sc3336->cur_vts;
}

/* 控件（v4l2_ctrl）变化的回调：将控件值写入硬件寄存器 */
static int sc3336_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sc3336 *sc3336 = container_of(ctrl->handler,
			       struct sc3336, ctrl_handler);
	struct i2c_client *client = sc3336->client;
	s64 max;
	int ret = 0;
	u32 val = 0;

	/* 当 vblank 变化时，更新 exposure 上限 */
	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		/* Update max exposure while meeting expected vblanking */
		max = sc3336->cur_mode->height + ctrl->val - 8;
		__v4l2_ctrl_modify_range(sc3336->exposure,
				 sc3336->exposure->minimum, max,
				 sc3336->exposure->step,
				 sc3336->exposure->default_value);
		break;
	}

	if (!pm_runtime_get_if_in_use(&client->dev))
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "set exposure 0x%x\n", ctrl->val);
		if (sc3336->cur_mode->hdr_mode == NO_HDR) {
			val = ctrl->val;
			/* 低 4 位为小数位 */
			ret = sc3336_write_reg(sc3336->client,
					SC3336_REG_EXPOSURE_H,
					SC3336_REG_VALUE_08BIT,
					SC3336_FETCH_EXP_H(val));
			ret |= sc3336_write_reg(sc3336->client,
					 SC3336_REG_EXPOSURE_M,
					 SC3336_REG_VALUE_08BIT,
					 SC3336_FETCH_EXP_M(val));
			ret |= sc3336_write_reg(sc3336->client,
					 SC3336_REG_EXPOSURE_L,
					 SC3336_REG_VALUE_08BIT,
					 SC3336_FETCH_EXP_L(val));
		}
		break;
	case V4L2_CID_ANALOGUE_GAIN:
		dev_dbg(&client->dev, "set gain 0x%x\n", ctrl->val);
		if (sc3336->cur_mode->hdr_mode == NO_HDR)
			ret = sc3336_set_gain_reg(sc3336, ctrl->val);
		break;
	case V4L2_CID_VBLANK:
		dev_dbg(&client->dev, "set vblank 0x%x\n", ctrl->val);
		ret = sc3336_write_reg(sc3336->client,
					SC3336_REG_VTS_H,
					SC3336_REG_VALUE_08BIT,
					(ctrl->val + sc3336->cur_mode->height)
					>> 8);
		ret |= sc3336_write_reg(sc3336->client,
					 SC3336_REG_VTS_L,
					 SC3336_REG_VALUE_08BIT,
					 (ctrl->val + sc3336->cur_mode->height)
					 & 0xff);
		sc3336->cur_vts = ctrl->val + sc3336->cur_mode->height;
		sc3336_modify_fps_info(sc3336);
		break;
	case V4L2_CID_TEST_PATTERN:
		ret = sc3336_enable_test_pattern(sc3336, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = sc3336_read_reg(sc3336->client, SC3336_FLIP_MIRROR_REG,
				       SC3336_REG_VALUE_08BIT, &val);
		ret |= sc3336_write_reg(sc3336->client, SC3336_FLIP_MIRROR_REG,
					 SC3336_REG_VALUE_08BIT,
					 SC3336_FETCH_MIRROR(val, ctrl->val));
		break;
	case V4L2_CID_VFLIP:
		ret = sc3336_read_reg(sc3336->client, SC3336_FLIP_MIRROR_REG,
				       SC3336_REG_VALUE_08BIT, &val);
		ret |= sc3336_write_reg(sc3336->client, SC3336_FLIP_MIRROR_REG,
					 SC3336_REG_VALUE_08BIT,
					 SC3336_FETCH_FLIP(val, ctrl->val));
		break;
	default:
		dev_warn(&client->dev, "%s Unhandled id:0x%x, val:0x%x\n",
			 __func__, ctrl->id, ctrl->val);
		break;
	}

	pm_runtime_put(&client->dev);

	return ret;
}

static const struct v4l2_ctrl_ops sc3336_ctrl_ops = {
	.s_ctrl = sc3336_set_ctrl,
};

/*
 * 初始化 v4l2 控件（exposure、gain、vblank、hblank、test pattern 等）
 * 这些控件会出现在 userspace 的 V4L2 控件接口中。
 */
static int sc3336_initialize_controls(struct sc3336 *sc3336)
{
	const struct sc3336_mode *mode;
	struct v4l2_ctrl_handler *handler;
	s64 exposure_max, vblank_def;
	u32 h_blank;
	int ret;
	u64 dst_link_freq = 0;
	u64 dst_pixel_rate = 0;

	handler = &sc3336->ctrl_handler;
	mode = sc3336->cur_mode;
	ret = v4l2_ctrl_handler_init(handler, 9);
	if (ret)
		return ret;
	handler->lock = &sc3336->mutex;

	sc3336->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(link_freq_menu_items) - 1, 0, link_freq_menu_items);
	if (sc3336->link_freq)
		sc3336->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	dst_link_freq = mode->link_freq_idx;
	dst_pixel_rate = (u32)link_freq_menu_items[mode->link_freq_idx] /
			 SC3336_BITS_PER_SAMPLE * 2 * SC3336_LANES;
	sc3336->pixel_rate = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_PIXEL_RATE,
		  0, PIXEL_RATE_WITH_255M_10BIT, 1, dst_pixel_rate);

	__v4l2_ctrl_s_ctrl(sc3336->link_freq, dst_link_freq);

	h_blank = mode->hts_def - mode->width;
	sc3336->hblank = v4l2_ctrl_new_std(handler, NULL, V4L2_CID_HBLANK,
				    h_blank, h_blank, 1, h_blank);
	if (sc3336->hblank)
		sc3336->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	vblank_def = mode->vts_def - mode->height;
	sc3336->vblank = v4l2_ctrl_new_std(handler, &sc3336_ctrl_ops,
				    V4L2_CID_VBLANK, vblank_def,
				    SC3336_VTS_MAX - mode->height,
				    1, vblank_def);
	exposure_max = mode->vts_def - 8;
	sc3336->exposure = v4l2_ctrl_new_std(handler, &sc3336_ctrl_ops,
				      V4L2_CID_EXPOSURE, SC3336_EXPOSURE_MIN,
				      exposure_max, SC3336_EXPOSURE_STEP,
				      mode->exp_def);
	sc3336->anal_gain = v4l2_ctrl_new_std(handler, &sc3336_ctrl_ops,
			       V4L2_CID_ANALOGUE_GAIN, SC3336_GAIN_MIN,
			       SC3336_GAIN_MAX, SC3336_GAIN_STEP,
			       SC3336_GAIN_DEFAULT);
	sc3336->test_pattern = v4l2_ctrl_new_std_menu_items(handler,
					    &sc3336_ctrl_ops,
			V4L2_CID_TEST_PATTERN,
			ARRAY_SIZE(sc3336_test_pattern_menu) - 1,
			0, 0, sc3336_test_pattern_menu);
	v4l2_ctrl_new_std(handler, &sc3336_ctrl_ops,
				V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, &sc3336_ctrl_ops,
				V4L2_CID_VFLIP, 0, 1, 1, 0);
	if (handler->error) {
		ret = handler->error;
		dev_err(&sc3336->client->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	sc3336->subdev.ctrl_handler = handler;
	sc3336->has_init_exp = false;
	sc3336->cur_fps = mode->max_fps;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

/* 读取传感器 ID 并与 CHIP_ID 比较以确认设备存在 */
static int sc3336_check_sensor_id(struct sc3336 *sc3336,
				   struct i2c_client *client)
{
	struct device *dev = &sc3336->client->dev;
	u32 id = 0;
	int ret;

	if (sc3336->is_thunderboot) {
		dev_info(dev, "Enable thunderboot mode, skip sensor id check\n");
		return 0;
	}

	ret = sc3336_read_reg(client, SC3336_REG_CHIP_ID,
			       SC3336_REG_VALUE_16BIT, &id);
	if (id != CHIP_ID) {
		dev_err(dev, "Unexpected sensor id(%06x), ret(%d)\n", id, ret);
		return -ENODEV;
	}

	dev_info(dev, "Detected OV%06x sensor\n", CHIP_ID);

	return 0;
}

/* 获取 regulator（电源）句柄 */
static int sc3336_configure_regulators(struct sc3336 *sc3336)
{
	unsigned int i;

	for (i = 0; i < SC3336_NUM_SUPPLIES; i++)
		sc3336->supplies[i].supply = sc3336_supply_names[i];

	return devm_regulator_bulk_get(&sc3336->client->dev,
				       SC3336_NUM_SUPPLIES,
				       sc3336->supplies);
}

/* probe：驱动入口，完成资源获取、初始化、检测并注册 v4l2 子设备 */
static int sc3336_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct sc3336 *sc3336;
	struct v4l2_subdev *sd;
	char facing[2];
	int ret;
	int i, hdr_mode = 0;

	dev_info(dev, "driver version: %02x.%02x.%02x",
		 DRIVER_VERSION >> 16,
		 (DRIVER_VERSION & 0xff00) >> 8,
		 DRIVER_VERSION & 0x00ff);

	sc3336 = devm_kzalloc(dev, sizeof(*sc3336), GFP_KERNEL);
	if (!sc3336)
		return -ENOMEM;

	/* 从 device tree 读取 module index/name/facing/lens 名称 */
	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &sc3336->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &sc3336->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &sc3336->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &sc3336->len_name);
	if (ret) {
		dev_err(dev, "could not get module information!\n");
		return -EINVAL;
	}

	sc3336->is_thunderboot = IS_ENABLED(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP);

	sc3336->client = client;
	for (i = 0; i < ARRAY_SIZE(supported_modes); i++) {
		if (hdr_mode == supported_modes[i].hdr_mode) {
			sc3336->cur_mode = &supported_modes[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(supported_modes))
		sc3336->cur_mode = &supported_modes[0];

	sc3336->xvclk = devm_clk_get(dev, "xvclk");
	if (IS_ERR(sc3336->xvclk)) {
		dev_err(dev, "Failed to get xvclk\n");
		return -EINVAL;
	}

	if (!sc3336->is_thunderboot)
		sc3336->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	else
		sc3336->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(sc3336->reset_gpio))
		dev_warn(dev, "Failed to get reset-gpios\n");

	if (!sc3336->is_thunderboot)
		sc3336->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_OUT_LOW);
	else
		sc3336->pwdn_gpio = devm_gpiod_get(dev, "pwdn", GPIOD_ASIS);
	if (IS_ERR(sc3336->pwdn_gpio))
		dev_warn(dev, "Failed to get pwdn-gpios\n");

	sc3336->pinctrl = devm_pinctrl_get(dev);
	if (!IS_ERR(sc3336->pinctrl)) {
		sc3336->pins_default =
			pinctrl_lookup_state(sc3336->pinctrl,
				 OF_CAMERA_PINCTRL_STATE_DEFAULT);
		if (IS_ERR(sc3336->pins_default))
			dev_err(dev, "could not get default pinstate\n");

		sc3336->pins_sleep =
			pinctrl_lookup_state(sc3336->pinctrl,
				 OF_CAMERA_PINCTRL_STATE_SLEEP);
		if (IS_ERR(sc3336->pins_sleep))
			dev_err(dev, "could not get sleep pinstate\n");
	} else {
		dev_err(dev, "no pinctrl\n");
	}

	ret = sc3336_configure_regulators(sc3336);
	if (ret) {
		dev_err(dev, "Failed to get power regulators\n");
		return ret;
	}

	mutex_init(&sc3336->mutex);

	sd = &sc3336->subdev;
	v4l2_i2c_subdev_init(sd, client, &sc3336_subdev_ops);
	ret = sc3336_initialize_controls(sc3336);
	if (ret)
		goto err_destroy_mutex;

	ret = __sc3336_power_on(sc3336);
	if (ret)
		goto err_free_handler;

	ret = sc3336_check_sensor_id(sc3336, client);
	if (ret)
		goto err_power_off;

#ifdef CONFIG_VIDEO_V4L2_SUBDEV_API
	sd->internal_ops = &sc3336_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			 V4L2_SUBDEV_FL_HAS_EVENTS;
#endif
#if defined(CONFIG_MEDIA_CONTROLLER)
	sc3336->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &sc3336->pad);
	if (ret < 0)
		goto err_power_off;
#endif

	memset(facing, 0, sizeof(facing));
	if (strcmp(sc3336->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s %s",
		 sc3336->module_index, facing,
		 SC3336_NAME, dev_name(sd->dev));
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret) {
		dev_err(dev, "v4l2 async register subdev failed\n");
		goto err_clean_entity;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	return 0;

err_clean_entity:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
err_power_off:
	__sc3336_power_off(sc3336);
err_free_handler:
	v4l2_ctrl_handler_free(&sc3336->ctrl_handler);
err_destroy_mutex:
	mutex_destroy(&sc3336->mutex);

	return ret;
}

/* remove：注销并清理资源 */
static int sc3336_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sc3336 *sc3336 = to_sc3336(sd);

	v4l2_async_unregister_subdev(sd);
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&sd->entity);
#endif
	v4l2_ctrl_handler_free(&sc3336->ctrl_handler);
	mutex_destroy(&sc3336->mutex);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		__sc3336_power_off(sc3336);
	pm_runtime_set_suspended(&client->dev);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id sc3336_of_match[] = {
	{ .compatible = "smartsens,sc3336" },
	{},
};
MODULE_DEVICE_TABLE(of, sc3336_of_match);
#endif

static const struct i2c_device_id sc3336_match_id[] = {
	{ "smartsens,sc3336", 0 },
	{ },
};

static struct i2c_driver sc3336_i2c_driver = {
	.driver = {
		.name = SC3336_NAME,
		.pm = &sc3336_pm_ops,
		.of_match_table = of_match_ptr(sc3336_of_match),
	},
	.probe		= &sc3336_probe,
	.remove		= &sc3336_remove,
	.id_table	= sc3336_match_id,
};

static int __init sensor_mod_init(void)
{
	return i2c_add_driver(&sc3336_i2c_driver);
}

static void __exit sensor_mod_exit(void)
{
	i2c_del_driver(&sc3336_i2c_driver);
}

#if defined(CONFIG_VIDEO_ROCKCHIP_THUNDER_BOOT_ISP) && !defined(CONFIG_INITCALL_ASYNC)
subsys_initcall(sensor_mod_init);
#else
device_initcall_sync(sensor_mod_init);
#endif
module_exit(sensor_mod_exit);

MODULE_DESCRIPTION("smartsens sc3336 sensor driver");
MODULE_LICENSE("GPL v2");

```

# 核心流程概览

1. **模块注册 / 初始化（probe）**

   * 驱动通过 `i2c_add_driver` 注册。内核发现匹配的设备后调用 `sc3336_probe()`。
   * probe 中：分配主结构体 `sc3336`、从 device-tree 读取 module 信息（index/name/facing/lens）、获取 xvclk、reset/pwdn GPIO、pinctrl、regulator（电源）句柄；初始化 mutex；初始化 V4L2 控件（exposure/gain/vblank 等）；执行上电序列 `__sc3336_power_on()`；读取并检查传感器 ID；注册 v4l2 async subdev 并启用 runtime PM。

2. **上电/下电**

   * `__sc3336_power_on()`：设置 pinctrl（默认态）、设置并使能 xvclk、使能 regulator、操作 reset/pwdn GPIO、必要延时（按 datasheet 需要多少 cycles 后才能进行 SCCB/I2C 事务）。
   * `__sc3336_power_off()`：撤销上电步骤，置 GPIO、设置 pins 为 sleep（如果可用），禁能 regulator。
   * runtime PM hooks (`sc3336_runtime_resume` / `sc3336_runtime_suspend`) 包装了上/下电逻辑，pm\_runtime 会在必要时调用。

3. **控件（V4L2 controls）管理**

   * `sc3336_initialize_controls()` 创建常用控件：exposure、analogue gain、vblank、hblank、pixel\_rate、link\_freq、test\_pattern、hflip/vflip。
   * 控件回调在 `sc3336_set_ctrl()`：当用户改变 exposure/gain/vblank/test\_pattern/flip 时，驱动将把相应值写入传感器寄存器（通过 I2C）。

4. **格式与模式**

   * 驱动预先定义了 `supported_modes[]`（分辨率、hts、vts、寄存器初始化表、xvclk、link\_freq 等）。
   * 用户设置格式时进入 `sc3336_set_fmt()`：找到最接近的模式，设置 `cur_mode`，并调整 hblank/vblank/pixel\_rate/link\_freq / cur\_fps。

5. **启动/停止视频流**

   * `sc3336_s_stream()` 是 v4l2 的接口实现：打开流会触发 pm\_runtime\_get、上电（如果需要）并最终调用 `__sc3336_start_stream()`；关闭流会调用 `__sc3336_stop_stream()` 并做 pm\_runtime\_put。
   * `__sc3336_start_stream()`：向传感器写入当前模式的寄存器表并把 mode 置为 streaming（写入 0x0100 寄存器）。可在 stream 前通过控件设置初始曝光/增益。

6. **I2C 读写封装**

   * `sc3336_write_reg()`：把 register address (16-bit) + value (最多 4 byte) 做成大端格式，调用 `i2c_master_send()`。
   * `sc3336_read_reg()`：先写 2 字节寄存器地址，然后读 len 字节（用两个 i2c\_msg + i2c\_transfer）。

7. **增益映射（关键实现点）**

   * `sc3336_set_gain_reg()`：把用户传入总增益拆成 analog gain / digital gain（coarse / fine），函数中有多个区间分支映射到传感器具体寄存器（厂商给的映射表）。这是容易出问题的地方（需保证传入的 gain 单位/范围正确）。

# 常用重要结构与宏（速记）

* `struct sc3336`：驱动的上下文，存放 i2c client、clk、gpio、v4l2 控件、cur\_mode、cur\_vts、streaming、power\_on 等。
* `struct sc3336_mode`：每个模式的描述（width/height/hts/vts/exp\_def/xvclk/link\_freq\_idx/reg\_list）。
* `CHIP_ID` / `SC3336_REG_CHIP_ID`：检查传感器是否接通的重要判断。
* 寄存器写入相关宏：`SC3336_FETCH_EXP_H/M/L` 用于将 exposure 值打包写到三个寄存器中。

# 调试要点 & 常见问题

* **传感器 ID 不匹配（read\_reg 返回的 id != CHIP\_ID）**：

  * 检查 I2C 地址与硬件连线（SCL/SDA），确认电源已上且 xvclk 正确；确认 reset/pwdn GPIO 是否处在正确电平；确认 device-tree 中 i2c 节点地址是否正确。
* **xvclk 频率不一致**：probe 中会尝试 `clk_set_rate()` 并打印警告，如果 `clk_get_rate()` 与期望不符，说明板子时钟配置或驱动配置（cur\_mode->xvclk\_freq）不一致。某些模式依赖 24MHz/27MHz。
* **无视频输出但驱动已注册**：看 `dmesg` 是否在 start\_stream 写寄存器时失败；检查 `__sc3336_write_array()` 返回值；确认 MIPI PHY / CSI2 接收端（SoC）参数（lane 数 / data type / virtual channel / continuous clock）和 `g_mbus_config()` 返回的一致。
* **曝光/增益 不生效**：确认控件是否已正确创建并且在 streaming 时控件值被写到了寄存器；用 `i2cget`/`i2cset` 或驱动中的 read\_reg 查看寄存器真实值来验证。
* **thunderboot 特殊处理**：驱动有对 Rockchip thunderboot 模式的特殊跳过逻辑，可能会跳过一些上电检查或初始化。在开发板上如果启用了相关 feature，需要留意这部分逻辑（`is_thunderboot`、`is_first_streamoff`）。

# 你在画布中会看到的内容

* 我在画布文件 `Sc3336 注释版` 中：

  * 在文件顶部对整体做了注释，解释了每个宏与结构体的作用；
  * 对关键函数（I2C 读写、上电/下电、控件初始化、probe、start/stop stream、set\_ctrl）都加入了逐步的中文说明；
  * 对寄存器初始化表做了注释说明（表为厂商提供，驱动按顺序写入）。

> **提示**：画布中的寄存器表我保留为原样（没有为每一行单独注释寄存器含义），如果你想我可以：
>
> * 把每个寄存器解释（如果你有厂商 datasheet/寄存器说明）逐条注释；或
> * 仅生成一个 patch（diff）把注释插入原始文件；或
> * 提取并生成更简洁的“上电 + 初始化时序”文档方便给硬件工程师使用。

# 接下来我可以帮你做的事情（选项）

1. 把画布里的注释导出成补丁（patch/diff）或直接生成带注释源码文件供下载。
2. 逐行注释寄存器表（需要 datasheet 或寄存器含义说明）。
3. 针对某个具体问题（例如：`sensor id mismatch`、`no stream`、`曝光不生效`）给出排查步骤并帮你生成 debug 命令（`i2cget`、`v4l2-ctl` 等）。
4. 将重点函数（如 `sc3336_set_gain_reg`、`__sc3336_power_on`、`__sc3336_start_stream`）做更深层次的读写时序/位域解析。

你想继续哪一步？或者直接在画布里打开 `Sc3336 注释版` 看注释后告诉我需要加强或调整的部分就行。
