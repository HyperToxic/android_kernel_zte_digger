/*
 *  drivers/media/radio/si470x/radio-si470x-common.c
 *
 *  Driver for radios with Silicon Labs Si470x FM Radio Receivers
 *
 *  Copyright (c) 2009 Tobias Lorenz <tobias.lorenz@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


/*
 * History:
 * 2008-01-12	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.0
 *		- First working version
 * 2008-01-13	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.1
 *		- Improved error handling, every function now returns errno
 *		- Improved multi user access (start/mute/stop)
 *		- Channel doesn't get lost anymore after start/mute/stop
 *		- RDS support added (polling mode via interrupt EP 1)
 *		- marked default module parameters with *value*
 *		- switched from bit structs to bit masks
 *		- header file cleaned and integrated
 * 2008-01-14	Tobias Lorenz <tobias.lorenz@gmx.net>
 * 		Version 1.0.2
 * 		- hex values are now lower case
 * 		- commented USB ID for ADS/Tech moved on todo list
 * 		- blacklisted si470x in hid-quirks.c
 * 		- rds buffer handling functions integrated into *_work, *_read
 * 		- rds_command in si470x_poll exchanged against simple retval
 * 		- check for firmware version 15
 * 		- code order and prototypes still remain the same
 * 		- spacing and bottom of band codes remain the same
 * 2008-01-16	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.3
 * 		- code reordered to avoid function prototypes
 *		- switch/case defaults are now more user-friendly
 *		- unified comment style
 *		- applied all checkpatch.pl v1.12 suggestions
 *		  except the warning about the too long lines with bit comments
 *		- renamed FMRADIO to RADIO to cut line length (checkpatch.pl)
 * 2008-01-22	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.4
 *		- avoid poss. locking when doing copy_to_user which may sleep
 *		- RDS is automatically activated on read now
 *		- code cleaned of unnecessary rds_commands
 *		- USB Vendor/Product ID for ADS/Tech FM Radio Receiver verified
 *		  (thanks to Guillaume RAMOUSSE)
 * 2008-01-27	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.5
 *		- number of seek_retries changed to tune_timeout
 *		- fixed problem with incomplete tune operations by own buffers
 *		- optimization of variables and printf types
 *		- improved error logging
 * 2008-01-31	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.6
 *		- fixed coverity checker warnings in *_usb_driver_disconnect
 *		- probe()/open() race by correct ordering in probe()
 *		- DMA coherency rules by separate allocation of all buffers
 *		- use of endianness macros
 *		- abuse of spinlock, replaced by mutex
 *		- racy handling of timer in disconnect,
 *		  replaced by delayed_work
 *		- racy interruptible_sleep_on(),
 *		  replaced with wait_event_interruptible()
 *		- handle signals in read()
 * 2008-02-08	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Oliver Neukum <oliver@neukum.org>
 *		Version 1.0.7
 *		- usb autosuspend support
 *		- unplugging fixed
 * 2008-05-07	Tobias Lorenz <tobias.lorenz@gmx.net>
 *		Version 1.0.8
 *		- hardware frequency seek support
 *		- afc indication
 *		- more safety checks, let si470x_get_freq return errno
 *		- vidioc behavior corrected according to v4l2 spec
 * 2008-10-20	Alexey Klimov <klimov.linux@gmail.com>
 * 		- add support for KWorld USB FM Radio FM700
 * 		- blacklisted KWorld radio in hid-core.c and hid-ids.h
 * 2008-12-03	Mark Lord <mlord@pobox.com>
 *		- add support for DealExtreme USB Radio
 * 2009-01-31	Bob Ross <pigiron@gmx.com>
 *		- correction of stereo detection/setting
 *		- correction of signal strength indicator scaling
 * 2009-01-31	Rick Bronson <rick@efn.org>
 *		Tobias Lorenz <tobias.lorenz@gmx.net>
 *		- add LED status output
 *		- get HW/SW version from scratchpad
 * 2009-06-16   Edouard Lafargue <edouard@lafargue.name>
 *		Version 1.0.10
 *		- add support for interrupt mode for RDS endpoint,
 *                instead of polling.
 *                Improves RDS reception significantly
 */


/* kernel includes */
#include "radio-si470x.h"
#include <linux/delay.h>


/**************************************************************************
 * Module Parameters
 **************************************************************************/

/* Spacing (kHz) */
/* 0: 200 kHz (USA, Australia) */
/* 1: 100 kHz (Europe, Japan) */
/* 2:  50 kHz */
static unsigned short space = 1;//wgh add it begin
module_param(space, ushort, 0444);
MODULE_PARM_DESC(space, "Spacing: 0=200kHz 1=100kHz *2=50kHz*");

/* Bottom of Band (MHz) */
/* 0: 87.5 - 108 MHz (USA, Europe)*/
/* 1: 76   - 108 MHz (Japan wide band) */
/* 2: 76   -  90 MHz (Japan) */
static unsigned short band = 0;//jiaobaocun modified for Europe
module_param(band, ushort, 0444);
MODULE_PARM_DESC(band, "Band: 0=87.5..108MHz *1=76..108MHz* 2=76..90MHz");

/* De-emphasis */
/* 0: 75 us (USA) */
/* 1: 50 us (Europe, Australia, Japan) */
static unsigned short de = 0;//lichuangchuang modified form 1
module_param(de, ushort, 0444);
MODULE_PARM_DESC(de, "De-emphasis: 0=75us *1=50us*");

/* Tune timeout */
static unsigned int tune_timeout = 3000;
//static unsigned int tune_timeout = 1000;
module_param(tune_timeout, uint, 0644);
MODULE_PARM_DESC(tune_timeout, "Tune timeout: *3000*");

/* Seek timeout */
static unsigned int seek_timeout = 5000;
//static unsigned int seek_timeout = 1000;
module_param(seek_timeout, uint, 0644);
MODULE_PARM_DESC(seek_timeout, "Seek timeout: *5000*");



/**************************************************************************
 * Generic Functions
 **************************************************************************/

/*
 * si470x_set_chan - set the channel
 */
static int si470x_set_chan(struct si470x_device *radio, unsigned short chan)
{
	int retval;
	unsigned long timeout;
	bool timed_out = 0;
	int i;
	/* start tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_CHAN;
	radio->registers[CHANNEL] |= CHANNEL_TUNE | chan;
	retval = si470x_set_register(radio, CHANNEL);
	if (retval < 0)
		goto done;

	/* currently I2C driver only uses interrupt way to tune */
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);

		pr_err("jiaobaocun tune \n");
		/* wait till tune operation has completed */
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(tune_timeout));
		if (!retval)
			timed_out = true;
	} else {
		/* wait till tune operation has completed */
		pr_err("jiaobaocun tune 2 \n");
		timeout = jiffies + msecs_to_jiffies(tune_timeout);
		do {
			retval = si470x_get_register(radio, STATUSRSSI);
			if (retval < 0)
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
		dev_err(&radio->videodev->dev, "jiaobaocun tune not timeout does not complete\n");
	if (timed_out)
		dev_err(&radio->videodev->dev,
			" jiaobaocun tune timed out after %u ms\n", tune_timeout);

		for(i=0; i < 16;i++)
		{
		retval = si470x_get_register(radio, i);
		}


stop:
	/* stop tuning */
	radio->registers[CHANNEL] &= ~CHANNEL_TUNE;
	retval = si470x_set_register(radio, CHANNEL);
	pr_err("jiaobaocun tune stop \n");
	retval = si470x_get_register(radio, 3);
done:
	return retval;
}


/*
 * si470x_get_freq - get the frequency
 */
static int si470x_get_freq(struct si470x_device *radio, unsigned int *freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;
	int retval;
	/* Spacing (kHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	/* 0: 200 kHz (USA, Australia) */
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	/* 1: 100 kHz (Europe, Japan) */
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	/* 2:  50 kHz */
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	/* Bottom of Band (MHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe) */
	case 0:
		band_bottom = 87.5 * FREQ_MUL; break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	default:
		band_bottom = 76   * FREQ_MUL; break;
	/* 2: 76   -  90 MHz (Japan) */
	case 2:
		band_bottom = 76   * FREQ_MUL; break;
	};

	/* read channel */
	retval = si470x_get_register(radio, READCHAN);
	chan = radio->registers[READCHAN] & READCHAN_READCHAN;

	/* Frequency (MHz) = Spacing (kHz) x Channel + Bottom of Band (MHz) */
	*freq = chan * spacing + band_bottom;
	printk("jiaobaocun si470x_get_freq, freq = %d. \n", (10 * (*freq) / 16000));
	return retval;
}


/*
 * si470x_set_freq - set the frequency
 */
int si470x_set_freq(struct si470x_device *radio, unsigned int freq)
{
	unsigned int spacing, band_bottom;
	unsigned short chan;
	printk("jiaobaocun si470x_set_freq, freq = %d, space = %d, band_bottom = %d. \n", (10 * freq / 16000), 
		((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4), ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6));
	/* Spacing (kHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_SPACE) >> 4) {
	/* 0: 200 kHz (USA, Australia) */
	case 0:
		spacing = 0.200 * FREQ_MUL; break;
	/* 1: 100 kHz (Europe, Japan) */
	case 1:
		spacing = 0.100 * FREQ_MUL; break;
	/* 2:  50 kHz */
	default:
		spacing = 0.050 * FREQ_MUL; break;
	};

	/* Bottom of Band (MHz) */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe) */
	case 0:
		band_bottom = 87.5 * FREQ_MUL;break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	default:
		band_bottom = 76   * FREQ_MUL;break;
	/* 2: 76   -  90 MHz (Japan) */
	case 2:
		band_bottom = 76   * FREQ_MUL;break;
	};

	/* Chan = [ Freq (Mhz) - Bottom of Band (MHz) ] / Spacing (kHz) */
	chan = (freq - band_bottom) / spacing;

	return si470x_set_chan(radio, chan);
}


/*
 * si470x_set_seek - set seek
 */
static int si470x_set_seek(struct si470x_device *radio,
		unsigned int wrap_around, unsigned int seek_upward)
{
	int retval = 0;
	unsigned long timeout;
	bool timed_out = 0;
	int i;
	/* start seeking */
	radio->registers[POWERCFG] |= POWERCFG_SEEK;
	if (wrap_around == 1)
		radio->registers[POWERCFG] &= ~POWERCFG_SKMODE;
	else
		radio->registers[POWERCFG] |= POWERCFG_SKMODE;
	if (seek_upward == 1)
		radio->registers[POWERCFG] |= POWERCFG_SEEKUP;
	else
		radio->registers[POWERCFG] &= ~POWERCFG_SEEKUP;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;

	/* currently I2C driver only uses interrupt way to seek */
	if (radio->stci_enabled) {
		INIT_COMPLETION(radio->completion);
		pr_err("jiaobaocun seek\n");
		/* wait till seek operation has completed */
		retval = wait_for_completion_timeout(&radio->completion,
				msecs_to_jiffies(seek_timeout));
		if (!retval)
			timed_out = true;
	} else {
		pr_err("jiaobaocun seek2\n");
		/* wait till seek operation has completed */
		timeout = jiffies + msecs_to_jiffies(seek_timeout);
		do {
			retval = si470x_get_register(radio, STATUSRSSI);
			if (retval < 0)
				goto stop;
			timed_out = time_after(jiffies, timeout);
		} while (((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0)
				&& (!timed_out));
	}

	if ((radio->registers[STATUSRSSI] & STATUSRSSI_STC) == 0){
		dev_err(&radio->videodev->dev, "jiaobaocun seek does not complete\n");
		retval = -7;
		goto stop;
	}
	if (radio->registers[STATUSRSSI] & STATUSRSSI_SF){
		dev_err(&radio->videodev->dev,
			"jiaobaocun seek failed / band limit reached\n");
		retval = -8;
		goto stop;
	}
	if (timed_out){
		dev_err(&radio->videodev->dev,
			"jiaobaocun seek timed out after %u ms\n", seek_timeout);
		retval = -9;
		goto stop;
	}

	for(i=0; i < 16;i++)
	{
		si470x_get_register(radio, i);
	}

stop:
	/* stop seeking */	
	radio->registers[POWERCFG] &= ~POWERCFG_SEEK;
	si470x_set_register(radio, POWERCFG);
	pr_err("jiaobaocun stop seek. wrap_around = %d, seek_upward = %d\n", wrap_around, seek_upward);
done:
	/* try again, if timed out */
	if ((retval == 0) && timed_out)
		retval = -EAGAIN;

	return retval;
}


/*
 * si470x_start - switch on radio
 */
int si470x_start(struct si470x_device *radio)
{
	int retval;
	printk("jiaobaocun si470x_start\n");
	/* powercfg */
	radio->registers[POWERCFG] =
		POWERCFG_DMUTE | POWERCFG_ENABLE | POWERCFG_RDSM;
	retval = si470x_set_register(radio, POWERCFG);
	if (retval < 0)
		goto done;
    msleep(110);

	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] =
		(de << 11) & SYSCONFIG1_DE;		/* DE*/
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	/* sysconfig 2 */
	radio->registers[SYSCONFIG2] =
		(SEEKTH_FM  << 8) |				/* SEEKTH *///lichuangchuang mod from 0x3f
		((band  << 6) & SYSCONFIG2_BAND)  |	/* BAND */
		((space << 4) & SYSCONFIG2_SPACE) |	/* SPACE */
		0;					/* VOLUME (max) */
	retval = si470x_set_register(radio, SYSCONFIG2);
	if (retval < 0)
		goto done;

	/* enable RDS / STC interrupt */
#ifdef USE_INITIAL_WAY         //lichuangchuang mod
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDSIEN;
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_STCIEN;
#else
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDSIEN;
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_STCIEN;
#endif
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_GPIO2;
	radio->registers[SYSCONFIG1] |= 0x1 << 2;
	retval = si470x_set_register(radio, SYSCONFIG1);
	/* reset last channel */
	retval = si470x_set_chan(radio,
		radio->registers[CHANNEL] & CHANNEL_CHAN);

done:
	return retval;
}


/*
 * si470x_stop - switch off radio
 */
int si470x_stop(struct si470x_device *radio)
{
	int retval;
	printk("jiaobaocun si470x_stop\n");
	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;

	/* powercfg */
	radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
	/* POWERCFG_ENABLE has to automatically go low */
	radio->registers[POWERCFG] |= POWERCFG_ENABLE |	POWERCFG_DISABLE;
	retval = si470x_set_register(radio, POWERCFG);

done:
	return retval;
}


/*
 * si470x_rds_on - switch on rds reception
 */
static int si470x_rds_on(struct si470x_device *radio)
{
	int retval;
	printk("jiaobaocun si470x_rds_on\n");
	/* sysconfig 1 */
	radio->registers[SYSCONFIG1] |= SYSCONFIG1_RDS;
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_RDS;

	return retval;
}



/**************************************************************************
 * File Operations Interface
 **************************************************************************/

/*
 * si470x_fops_read - read RDS data
 */
static ssize_t si470x_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	unsigned int block_count = 0;
	printk("jiaobaocun si470x_fops_read\n");
	/* switch on rds reception */
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio);

	/* block if no new data available */
	while (radio->wr_index == radio->rd_index) {
		if (file->f_flags & O_NONBLOCK) {
			retval = -EWOULDBLOCK;
			goto done;
		}
		if (wait_event_interruptible(radio->read_queue,
			radio->wr_index != radio->rd_index) < 0) {
			retval = -EINTR;
			goto done;
		}
	}

	/* calculate block count from byte count */
	count /= 3;

	/* copy RDS block out of internal buffer and to user buffer */
	while (block_count < count) {
		if (radio->rd_index == radio->wr_index)
			break;

		/* always transfer rds complete blocks */
		if (copy_to_user(buf, &radio->buffer[radio->rd_index], 3))
			/* retval = -EFAULT; */
			break;

		/* increment and wrap read pointer */
		radio->rd_index += 3;
		if (radio->rd_index >= radio->buf_size)
			radio->rd_index = 0;

		/* increment counters */
		block_count++;
		buf += 3;
		retval += 3;
	}

done:
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_fops_poll - poll RDS data
 */
static unsigned int si470x_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	/* switch on rds reception */
	printk("jiaobaocun si470x_fops_poll\n");
	mutex_lock(&radio->lock);
	if ((radio->registers[SYSCONFIG1] & SYSCONFIG1_RDS) == 0)
		si470x_rds_on(radio);
	mutex_unlock(&radio->lock);

	poll_wait(file, &radio->read_queue, pts);

	if (radio->rd_index != radio->wr_index)
		retval = POLLIN | POLLRDNORM;

	return retval;
}


/*
 * si470x_fops - file operations interface
 */
static const struct v4l2_file_operations si470x_fops = {
	.owner			= THIS_MODULE,
	.read			= si470x_fops_read,
	.poll			= si470x_fops_poll,
	.unlocked_ioctl		= video_ioctl2,
	.open			= si470x_fops_open,
	.release		= si470x_fops_release,
};



/**************************************************************************
 * Video4Linux Interface
 **************************************************************************/

/*
 * si470x_vidioc_queryctrl - enumerate control items
 */
static int si470x_vidioc_queryctrl(struct file *file, void *priv,
		struct v4l2_queryctrl *qc)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = -EINVAL;
	printk("jiaobaocun si470x_vidioc_queryctrl\n");
	/* abort if qc->id is below V4L2_CID_BASE */
	if (qc->id < V4L2_CID_BASE)
		goto done;

	/* search video control */
	switch (qc->id) {
	case V4L2_CID_AUDIO_VOLUME:
		return v4l2_ctrl_query_fill(qc, 0, 15, 1, 15);
	case V4L2_CID_AUDIO_MUTE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	}

	/* disable unsupported base controls */
	/* to satisfy kradio and such apps */
	if ((retval == -EINVAL) && (qc->id < V4L2_CID_LASTP1)) {
		qc->flags = V4L2_CTRL_FLAG_DISABLED;
		retval = 0;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"query controls failed with %d\n", retval);
	return retval;
}


/*
 * si470x_vidioc_g_ctrl - get the value of a control
 */
static int si470x_vidioc_g_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_g_ctrl\n");
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = radio->registers[SYSCONFIG2] &
				SYSCONFIG2_VOLUME;
		break;
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = ((radio->registers[POWERCFG] &
				POWERCFG_DMUTE) == 0) ? 1 : 0;
		break;
	default:
		retval = -EINVAL;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get control failed with %d\n", retval);

	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_ctrl - set the value of a control
 */
static int si470x_vidioc_s_ctrl(struct file *file, void *priv,
		struct v4l2_control *ctrl)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_s_ctrl\n");
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_VOLUME:
		radio->registers[SYSCONFIG2] &= ~SYSCONFIG2_VOLUME;
		radio->registers[SYSCONFIG2] |= ctrl->value;
		retval = si470x_set_register(radio, SYSCONFIG2);
		pr_err("jiaobaocun si470x_vidioc_s_ctrl V4L2_CID_AUDIO_VOLUME ctrl->value = %d\n",ctrl->value);
		break;
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value == 1)
			radio->registers[POWERCFG] &= ~POWERCFG_DMUTE;
		else
			radio->registers[POWERCFG] |= POWERCFG_DMUTE;
		retval = si470x_set_register(radio, POWERCFG);
		pr_err("jiaobaocun si470x_vidioc_s_ctrl V4L2_CID_AUDIO_MUTE\n");
		break;
	default:
		pr_err("jiaobaocun err_s_ctrl return 0\n");
		//luxiazi modify
		//retval = -EINVAL;
		retval = 0;
	}

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set control failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_g_audio - get audio attributes
 */
static int si470x_vidioc_g_audio(struct file *file, void *priv,
		struct v4l2_audio *audio)
{
	/* driver constants */
	audio->index = 0;
	strcpy(audio->name, "Radio");
	audio->capability = V4L2_AUDCAP_STEREO;
	audio->mode = 0;
	printk("jiaobaocun si470x_vidioc_g_audio\n");
	return 0;
}


/*
 * si470x_vidioc_g_tuner - get tuner attributes
 */
static int si470x_vidioc_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_g_tuner\n");
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0) {
		retval = -EINVAL;
		goto done;
	}

	retval = si470x_get_register(radio, STATUSRSSI);
	if (retval < 0)
		goto done;

	/* driver constants */
	strcpy(tuner->name, "FM");
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW | V4L2_TUNER_CAP_STEREO |
			    V4L2_TUNER_CAP_RDS | V4L2_TUNER_CAP_RDS_BLOCK_IO;

	/* range limits */
	switch ((radio->registers[SYSCONFIG2] & SYSCONFIG2_BAND) >> 6) {
	/* 0: 87.5 - 108 MHz (USA, Europe, default) */
	default:
		tuner->rangelow  =  87.5 * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	/* 1: 76   - 108 MHz (Japan wide band) */
	case 1:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh = 108   * FREQ_MUL;
		break;
	/* 2: 76   -  90 MHz (Japan) */
	case 2:
		tuner->rangelow  =  76   * FREQ_MUL;
		tuner->rangehigh =  90   * FREQ_MUL;
		break;
	};

	/* stereo indicator == stereo (instead of mono) */
	if ((radio->registers[STATUSRSSI] & STATUSRSSI_ST) == 0)
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO;
	else
		tuner->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	/* If there is a reliable method of detecting an RDS channel,
	   then this code should check for that before setting this
	   RDS subchannel. */
	tuner->rxsubchans |= V4L2_TUNER_SUB_RDS;

	/* mono/stereo selector */
	if ((radio->registers[POWERCFG] & POWERCFG_MONO) == 0)
		tuner->audmode = V4L2_TUNER_MODE_STEREO;
	else
		tuner->audmode = V4L2_TUNER_MODE_MONO;

	/* min is worst, max is best; signal:0..0xffff; rssi: 0..0xff */
	/* measured in units of dbµV in 1 db increments (max at ~75 dbµV) */
	tuner->signal = (radio->registers[STATUSRSSI] & STATUSRSSI_RSSI);
	/* the ideal factor is 0xffff/75 = 873,8 */
	tuner->signal = (tuner->signal * 873) + (8 * tuner->signal / 10);

	/* automatic frequency control: -1: freq to low, 1 freq to high */
	/* AFCRL does only indicate that freq. differs, not if too low/high */
	tuner->afc = (radio->registers[STATUSRSSI] & STATUSRSSI_AFCRL) ? 1 : 0;

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get tuner failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_tuner - set tuner attributes
 */
static int si470x_vidioc_s_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_s_tuner\n");
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (tuner->index != 0){
		printk("jiaobaocun si470x_vidioc_s_tuner tuner->index != 0\n");
		retval = -EINVAL;
		goto done;
	}
	
	de = tuner->reserved[0];
	/* set de*/
	radio->registers[SYSCONFIG1] &= ~SYSCONFIG1_DE;		/* DE*/
	radio->registers[SYSCONFIG1] |= (de << 11) & SYSCONFIG1_DE;		/* DE*/
	retval = si470x_set_register(radio, SYSCONFIG1);
	if (retval < 0)
		goto done;
	space = tuner->reserved[1];
	if((tuner->rangelow == 87500) && (tuner->rangehigh == 108000))
		band = 0;
	else if((tuner->rangelow == 76000) && (tuner->rangehigh == 108000))
		band = 1;
	else if((tuner->rangelow == 76000) && (tuner->rangehigh == 90000))
		band = 2;
	else
		band = 0;
	/* set band & space */
	radio->registers[SYSCONFIG2] &=
		~((SYSCONFIG2_BAND)  |	/* BAND */
		(SYSCONFIG2_SPACE)); 	/* SPACE */
	radio->registers[SYSCONFIG2] |=
		((band  << 6) & SYSCONFIG2_BAND)  |	/* BAND */
		((space << 4) & SYSCONFIG2_SPACE); 	/* SPACE */
	retval = si470x_set_register(radio, SYSCONFIG2);
	if (retval < 0)
		goto done;

	printk("jiaobaocun si470x_FM_set_mode, audmode = %d, de = %d, band = %d, space = %d. \n", tuner->audmode, de, band, space);
	
	/* mono/stereo selector */
	switch (tuner->audmode) {
	case V4L2_TUNER_MODE_MONO:
		radio->registers[POWERCFG] |= POWERCFG_MONO;  /* force mono */
		break;
	case V4L2_TUNER_MODE_STEREO:
		radio->registers[POWERCFG] &= ~POWERCFG_MONO; /* try stereo */
		break;
	default:
		goto done;
	}

	retval = si470x_set_register(radio, POWERCFG);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set tuner failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_g_frequency - get tuner or modulator radio frequency
 */
static int si470x_vidioc_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_g_frequency\n");
	/* safety checks */
	mutex_lock(&radio->lock);
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (freq->tuner != 0) {
		//retval = -EINVAL;
		//goto done;
	}

	freq->type = V4L2_TUNER_RADIO;
	retval = si470x_get_freq(radio, &freq->frequency);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"get frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_frequency - set tuner or modulator radio frequency
 */
static int si470x_vidioc_s_frequency(struct file *file, void *priv,
		struct v4l2_frequency *freq)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (freq->tuner != 0) {

		printk("jiaobaocun freq->tuner != 0\n");
		//retval = -EINVAL;
		//goto done;
	}
	
	retval = si470x_set_freq(radio, freq->frequency);
	//jiaobaocun added
	//radio->seek_completion = 0;
done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set frequency failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_vidioc_s_hw_freq_seek - set hardware frequency seek
 */
static int si470x_vidioc_s_hw_freq_seek(struct file *file, void *priv,
		struct v4l2_hw_freq_seek *seek)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;
	printk("jiaobaocun si470x_vidioc_s_hw_freq_seek\n");
	mutex_lock(&radio->lock);
	/* safety checks */
	retval = si470x_disconnect_check(radio);
	if (retval)
		goto done;

	if (seek->tuner != 0) {
		printk("jiaobaocun seek->tuner==0\n");
		//retval = -EINVAL;
		//goto done;
	}

	retval = si470x_set_seek(radio, seek->wrap_around, seek->seek_upward);

done:
	if (retval < 0)
		dev_warn(&radio->videodev->dev,
			"set hardware frequency seek failed with %d\n", retval);
	mutex_unlock(&radio->lock);
	return retval;
}


/*
 * si470x_ioctl_ops - video device ioctl operations
 */
static const struct v4l2_ioctl_ops si470x_ioctl_ops = {
	.vidioc_querycap	= si470x_vidioc_querycap,
	.vidioc_queryctrl	= si470x_vidioc_queryctrl,
	.vidioc_g_ctrl		= si470x_vidioc_g_ctrl,
	.vidioc_s_ctrl		= si470x_vidioc_s_ctrl,
	.vidioc_g_audio		= si470x_vidioc_g_audio,
	.vidioc_g_tuner		= si470x_vidioc_g_tuner,
	.vidioc_s_tuner		= si470x_vidioc_s_tuner,
	.vidioc_g_frequency	= si470x_vidioc_g_frequency,
	.vidioc_s_frequency	= si470x_vidioc_s_frequency,
	.vidioc_s_hw_freq_seek	= si470x_vidioc_s_hw_freq_seek,
};


/*
 * si470x_viddev_template - video device interface
 */
struct video_device si470x_viddev_template = {
	.fops			= &si470x_fops,
	.name			= DRIVER_NAME,
	.release		= video_device_release,
	.ioctl_ops		= &si470x_ioctl_ops,
};
