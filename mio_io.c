//****************************************************************************
//	
//	Copyright 2010-12 by WinSystems Inc.
//
//	Permission is hereby granted to the purchaser of WinSystems GPIO cards 
//	and CPU products incorporating a GPIO device, to distribute any binary 
//	file or files compiled using this source code directly or in any work 
//	derived by the user from this file. In no case may the source code, 
//	original or derived from this file, be distributed to any third party 
//	except by explicit permission of WinSystems. This file is distributed 
//	on an "As-is" basis and no warranty as to performance or fitness of pur-
//	poses is expressed or implied. In no case shall WinSystems be liable for 
//	any direct or indirect loss or damage, real or consequential resulting 
//	from the usage of this source code. It is the user's sole responsibility 
//	to determine fitness for any considered purpose.
//
//****************************************************************************
//
//	Name	 : mio_io.c
//
//	Project	 : PCMMIO Linux Device Driver
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	08/30/11	  2.1		Fixed bug in write_dio_byte function	
//	10/09/12	  3.0		Added comments for all functions		
//
//****************************************************************************

#define LIB_DEFINED

#include "mio_io.h"    

#include <stdio.h>
#include <fcntl.h>      // open  
#include <unistd.h>     // exit 
#include <sys/ioctl.h>  // ioctl 

// These image variable help out where a register is not
// capable of a read/modify/write operation 
unsigned char dio_port_images[MAX_DEV][11];
unsigned char adc1_port_image[MAX_DEV] = {0, 0, 0, 0};
unsigned char adc2_port_image[MAX_DEV] = {0, 0, 0, 0};
unsigned char dac1_port_image[MAX_DEV] = {0, 0, 0, 0};
unsigned char dac2_port_image[MAX_DEV] = {0, 0, 0, 0};

// The channel selects on the ADC are non contigous. In order to avoid shifting and such
// with each channel select, we simple bild an array for selection. 
unsigned char adc_channel_select[MAX_DEV][16] = {
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT };

// Mode selection can also be time consuming and we'd also like the mode to "Stick" from
// call to call. This array will eventually undefined reference to `main'hold the actual command byte to send to the
// ADC controller for each channel according to the mode set with adc_set_channel_mode 
unsigned char adc_channel_mode[MAX_DEV][16] = {
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT, 
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT, 
	ADC_CH4_SELECT, ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT,
	ADC_CH0_SELECT, ADC_CH1_SELECT, ADC_CH2_SELECT, ADC_CH3_SELECT,
	ADC_CH4_SELECT,	ADC_CH5_SELECT, ADC_CH6_SELECT, ADC_CH7_SELECT };

// This array and the index value are used internally for the adc_convert_all_channels
// and for the adc_buffered_conversion routines. 
unsigned char adc_channel_buff[18] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,0xff,0xff};
unsigned char *adc_input_buffer;
unsigned short *adc_user_buffer;

// Becaues of the nature of the ADC beast. It's necessary to keep track of the last
// channel and the previous channel in order to retrieve the data amd know who it belongs to 
int adc_last_channel[MAX_DEV], adc_current_channel[MAX_DEV];

// Various index pointers for the above arrays and misc globals 
int adc_ch_index[MAX_DEV] = {0, 0, 0, 0};
int adc_out_index[MAX_DEV] = {0, 0, 0, 0};
int adc_repeat_channel[MAX_DEV];
int adc_repeat_count[MAX_DEV];

//------------------------------------------------------------------------
//		USER LIBRARY FUNCTIONS
//------------------------------------------------------------------------

int handle[MAX_DEV] = {0, 0, 0, 0};
char *device_id[MAX_DEV] ={"/dev/pcmmio_wsa",
						   "/dev/pcmmio_wsb",
						   "/dev/pcmmio_wsc",
						   "/dev/pcmmio_wsd"};

//------------------------------------------------------------------------
//
// disable_dio_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int disable_dio_interrupt(int dev_num)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available 
		return -1;

	adc1_port_image[dev_num] = adc1_port_image[dev_num] | 0x10;
	mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Access the int enable register
	mio_write_reg(dev_num, 0x02, 0);
	adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xef;
	mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Disable the interrupt
	
	return 0;
}

//------------------------------------------------------------------------
//
// enable_dio_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int enable_dio_interrupt(int dev_num)
{
	unsigned char vector;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available 
		return -1;

	// We read the assign IRQ from the driver, so we can program the hardware to
	// match,  but only if an aaplication desires such. 
	vector = mio_read_irq_assigned(dev_num);

	if(vector == 0)
	{
		mio_error_code = MIO_MISSING_IRQ;
		sprintf(mio_error_string,"MIO(DIO) : enable_dio_interrupt - No IRQ assigned\n");
		return 1;
	}

	adc1_port_image[dev_num] = adc1_port_image[dev_num] | 0x10;
	mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Access the int enable register
	mio_write_reg(dev_num, 0x02, vector);
	adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xef;
	mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Enable the interrupt

	return 0;
}

//------------------------------------------------------------------------
//
// disable_dac_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int disable_dac_interrupt(int dev_num, int dac_num)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	if(dac_num)
	{
		dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xfe;
		dac2_port_image[dev_num] = dac2_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x0f, dac2_port_image[dev_num]);	// Access the int enable register
		mio_write_reg(dev_num, 0x0e, 0);
		dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xf7;
		mio_write_reg(dev_num, 0x0f, dac2_port_image[dev_num]);	// Disable the interrupt
	}
	else
	{
		dac1_port_image[dev_num] = dac1_port_image[dev_num] & 0xfe;
		dac1_port_image[dev_num] = dac1_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x0b, dac1_port_image[dev_num]);	// Access the int enable register
	
		mio_write_reg(dev_num, 0x0a, 0);

		dac1_port_image[dev_num] = dac1_port_image[dev_num] & 0xf7;
		mio_write_reg(dev_num, 0x0b, dac1_port_image[dev_num]);	// Disable the interrupt
	}

	return 0;
}

//------------------------------------------------------------------------
//
// enable_dac_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Returns:
//			0	The function completes successfully
//			1	No IRQ assigned
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int enable_dac_interrupt(int dev_num, int dac_num)
{
	unsigned char vector;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	// We read the assign IRQ from the driver, so we can program the hardware to
	// match,  but only if an aaplication desires such.
	vector = mio_read_irq_assigned(dev_num);

	if(vector == 0)
	{
		mio_error_code = MIO_MISSING_IRQ;
		sprintf(mio_error_string,"MIO(DAC) : enable_dac_interrupt - No IRQ assigned\n");
		return 1;
	}

	if(dac_num)
	{
		dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xfe;
		dac2_port_image[dev_num] = dac2_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x0f, dac2_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x0e, vector);

		dac2_port_image[dev_num] = dac2_port_image[dev_num] & 0xf7;
		dac2_port_image[dev_num] = dac2_port_image[dev_num] | 0x01;
		mio_write_reg(dev_num, 0x0f, dac2_port_image[dev_num]);	// Enable the interrupt
	}
	else
	{
		dac1_port_image[dev_num] = dac1_port_image[dev_num] & 0xfe;
		dac1_port_image[dev_num] = dac1_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x0b, dac1_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x0a, vector);

		dac1_port_image[dev_num] = dac1_port_image[dev_num] & 0xf7;
		dac1_port_image[dev_num] = dac1_port_image[dev_num] | 0x01;
		mio_write_reg(dev_num, 0x0b, dac1_port_image[dev_num]);	// Enable the interrupt
	}

	return 0;
}

//------------------------------------------------------------------------
//
// disable_adc_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int disable_adc_interrupt(int dev_num, int adc_num)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	if(adc_num)
	{
		adc2_port_image[dev_num] = adc2_port_image[dev_num] & 0xfe;
		adc2_port_image[dev_num] = adc2_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x07, adc2_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x06, 0);

		adc2_port_image[dev_num] = adc2_port_image[dev_num] & 0xf7;
		mio_write_reg(dev_num, 0x07, adc2_port_image[dev_num]);	// Disable the interrupt
	}
	else
	{
		adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xfe;
		adc1_port_image[dev_num] = adc1_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x02, 0);

		adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xf7;
		mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Disable the interrupt
	}

	return 0;
}

//------------------------------------------------------------------------
//
// enable_adc_interrupt
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Returns:
//			0	The function completes successfully
//			1	No IRQ assigned
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int enable_adc_interrupt(int dev_num, int adc_num)
{
	unsigned char vector;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	// We read the assign IRQ from the driver, so we can program the hardware to
	// match,  but only if an aaplication desires such.
	vector = mio_read_irq_assigned(dev_num);

	if(vector == 0)
	{
		mio_error_code = MIO_MISSING_IRQ;
		sprintf(mio_error_string,"MIO(ADC) : enable_adc_interrupt - No IRQ assigned\n");
		return 1;
	}

	if(adc_num)
	{
		adc2_port_image[dev_num] = adc2_port_image[dev_num] & 0xfe;
		adc2_port_image[dev_num] = adc2_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x07, adc2_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x06, vector);

		adc2_port_image[dev_num] = adc2_port_image[dev_num] & 0xf7;
		adc2_port_image[dev_num] = adc2_port_image[dev_num] | 0x01;
		mio_write_reg(dev_num, 0x07, adc2_port_image[dev_num]);	// Enable the interrupt
	}
	else
	{
		adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xfe;
		adc1_port_image[dev_num] = adc1_port_image[dev_num] | 0x08;
		mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Access the int enable register

		mio_write_reg(dev_num, 0x02, vector);

		adc1_port_image[dev_num] = adc1_port_image[dev_num] & 0xf7;
		adc1_port_image[dev_num] = adc1_port_image[dev_num] | 0x01;
		mio_write_reg(dev_num, 0x03, adc1_port_image[dev_num]);	// Enable the interrupt
	}

	return 0;
}

//------------------------------------------------------------------------
//
// mio_read_irq_assigned
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			IRQ Integer value of IRQ
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int mio_read_irq_assigned(int dev_num)
{
	unsigned char val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	// All of our programming of the hardware is handled at this level so that 
	// all of the routines that need to shove and IRQ value into hardware will 
	// use this call.
    val = ioctl(handle[dev_num], READ_IRQ_ASSIGNED, NULL);

	return (val & 0xff);
}

//------------------------------------------------------------------------
//
// set_dac_span
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			span_value	Desired value
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int set_dac_span(int dev_num, int channel, unsigned char span_value)
{
	unsigned char select_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available
		return -1;

	if((channel < 0) || (channel > 7))
	{
		mio_error_code = MIO_BAD_CHANNEL_NUMBER;
		sprintf(mio_error_string,"MIO(DAC) : Set_dac_span - bad channel number %d\n",channel);
		return 1;
	}

	// This function sets up the output range for the DAC channel
	select_val = (channel % 4) << 1;

	write_dac_data(dev_num,  channel / 4, span_value);
	if(mio_error_code)
		return 1;

	write_dac_command(dev_num,  channel / 4, 0x60 | select_val);
	if(mio_error_code)
		return 1;

	if(wait_dac_ready(dev_num, channel))
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// wait_dac_ready
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//
// Returns:
//			0	The function completes successfully
//			1	Time-out waiting for DAC
//
//------------------------------------------------------------------------
int wait_dac_ready(int dev_num, int channel)
{
	unsigned long retry;

	retry = 100000L;

	// This may seem like an absurd way to handle waiting and violates the
	// "no busy waiting" policy. The fact is that the hardware is normally so fast that we
	// usually only need one time through the loop anyway. The longer timeout is for rare
	// occasions and for detecting non-existent hardware.  
	while(retry--)
	{
		if(dac_read_status(dev_num,  channel / 4) & DAC_BUSY)
			return 0;
	}

	return 1;
}

//------------------------------------------------------------------------
//
// set_dac_output
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			dac_value	Desired output
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int set_dac_output(int dev_num, int channel, unsigned short dac_value)
{
	unsigned char select_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	select_val = (channel % 4) << 1;

	write_dac_data(dev_num,  channel / 4, dac_value);
	if(mio_error_code)
		return 1;

	write_dac_command(dev_num,  channel / 4, 0x70 | select_val);
	if(mio_error_code)
		return 1;

	if(wait_dac_ready(dev_num, channel))
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// set_dac_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		DAC channel
//			voltage		Desired voltage
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int set_dac_voltage(int dev_num, int channel, float voltage)
{
	unsigned short value;
	float bit_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// This output function is auto-ranging in that it picks the span that will
	// give the most precision for the voltage specified. This has one side-effect that
	// may be objectionable to some applications. When call to set_dac_span completes the
	// new range is set and the output will respond immediately using whatever value was last
	// in the output registers. This may cause a spike (up or down) in the DAC output until the
	// new output value is sent to the controller.  
	if((voltage < -10.0) || (voltage > 10.0))
	{
		mio_error_code = MIO_ILLEGAL_VOLTAGE;
		sprintf(mio_error_string,"MIO(DAC) :Set DAC Voltage - Illegal Voltage %9.5f\n",voltage);
		return 1;
	}

	if((voltage >= 0.0) && (voltage < 5.0))
	{
		set_dac_span(dev_num, channel, DAC_SPAN_UNI5);
		if(mio_error_code)
			return 1;
		bit_val = 5.0 / 65536;
		value = (unsigned short) (voltage / bit_val);
	}

	if(voltage >= 5.0)
	{
		set_dac_span(dev_num, channel, DAC_SPAN_UNI10);
		if(mio_error_code)
			return 1;
		bit_val = 10.0 / 65536;
		value = (unsigned short) (voltage / bit_val);
	}

	if((voltage < 0.0) && (voltage > -5.0))
	{
		set_dac_span(dev_num, channel, DAC_SPAN_BI5);
		if(mio_error_code)
			return 1;
		bit_val = 10.0 / 65536;
		value = (unsigned short) ((voltage + 5.0) / bit_val);
	}

	if(voltage <= -5.0)
	{
		set_dac_span(dev_num, channel, DAC_SPAN_BI10);
		if(mio_error_code)
			return 1;
		bit_val = 20.0 / 65536;
		value  = (unsigned short) ((voltage + 10.0) / bit_val);
	}

	if(wait_dac_ready(dev_num, channel))
		return 1;

	set_dac_output(dev_num, channel, value);
	if(mio_error_code)
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// read_dio_byte
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//
// Returns:
//			VAL Value read from the selected byte
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
unsigned char read_dio_byte(int dev_num, int offset)
{
	int val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// All bit operations are handled at this level so we need only
	// read and write bytes from the actual hardware using the driver
	// to handle our ioctl call for it.  
    val = ioctl(handle[dev_num], READ_DIO_BYTE, offset);

	return (unsigned char) (val & 0xff);
}

//------------------------------------------------------------------------
//
// mio_read_reg
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//
// Returns:
//			VAL Value read from the selected byte
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
unsigned char mio_read_reg(int dev_num, int offset)
{
	int val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// This is a catchall register read routine that allows reading of
	// ANY of the registers on the PCM-MIO. It is used primarily for
	// retreiving control and access values in the hardware.  
	val = ioctl(handle[dev_num], MIO_READ_REG, offset);

	return (unsigned char) (val & 0xff);
}

//------------------------------------------------------------------------
//
// mio_write_reg
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//			value		New register value
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int mio_write_reg(int dev_num, int offset, unsigned char value)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// This function like the previous allow unlimited
	// write access to ALL of the registers on the PCM-MIO  
	ioctl(handle[dev_num], MIO_WRITE_REG, (value << 8) | offset);
	
	return 0;
}

//------------------------------------------------------------------------
//
// write_dio_byte
//
// Arguments:
//			dev_num		The index of the chip
//			offset		Register offset
//			value		New register value
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int write_dio_byte(int dev_num, int offset, unsigned char value)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// PBD 8-30-11 Moved from dio_write_bit
	// update image
	dio_port_images[dev_num][offset] = value;

	// All bit operations for the DIO are handled at this level
	// and we need the driver to allow access to the actual
	// DIO registers to update the value.  
	ioctl(handle[dev_num], WRITE_DIO_BYTE, (value << 8) | offset);
	
	return 0;
}

//------------------------------------------------------------------------
//
// write_dac_command
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//			value		DAC command
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int write_dac_command(int dev_num, int dac_num, unsigned char value)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	ioctl(handle[dev_num], WRITE_DAC_COMMAND, (value << 8) | dac_num);

	return 0;
}

//------------------------------------------------------------------------
//
// adc_start_conversion
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int adc_start_conversion(int dev_num, int channel)
{
	mio_error_code = MIO_SUCCESS;

	if(dev_num < 0 || dev_num > MAX_DEV)
	{
		mio_error_code = MIO_BAD_DEVICE;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Device Number %d\n",dev_num);
		return 1;
	}

	if((channel <0) || (channel > 15))
	{
		mio_error_code = MIO_BAD_CHANNEL_NUMBER;
		sprintf(mio_error_string,"MIO(ADC) : Start conversion bad channel number %d\n",channel);
		return 1;
	}

	adc_last_channel[dev_num] = adc_current_channel[dev_num];
	adc_current_channel[dev_num] = channel;

	write_adc_command(dev_num, channel / 8, adc_channel_mode[dev_num][channel]);
	
	if(mio_error_code)
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// adc_get_channel_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			VAL	Voltage of selected channel
//			0	Error
//
//------------------------------------------------------------------------
float adc_get_channel_voltage(int dev_num, int channel)
{
	unsigned short value;
	float result;

	mio_error_code = MIO_SUCCESS;

	// Start two conversions so that we can have current data
	adc_start_conversion(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_start_conversion(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	// Read out the conversion's raw data
	value = adc_read_conversion_data(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	// Convert the raw data to a voltage 
	value = value + adc_adjust[dev_num][channel];
	result = value * adc_bitval[dev_num][channel];
	result = result + adc_offset[dev_num][channel];

	return result;
}

//------------------------------------------------------------------------
//
// adc_convert_all_channels
//
// Arguments:
//			dev_num		The index of the chip
//			buffer		Storage of channel data
//
// Returns:
//			0	The function completes successfully
//			1	Error
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int adc_convert_all_channels(int dev_num, unsigned short *buffer)
{
	int x;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Initialize global variables including transferinng the
	// address of the user's ouput buffer to an internal buffer pointer
	adc_user_buffer = buffer;
	adc_input_buffer = adc_channel_buff;
	adc_ch_index[dev_num] =0;
	adc_out_index[dev_num] = 0;

	adc_start_conversion(dev_num, 0);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, 0);
	if(mio_error_code)
		return 1;

	// This is old data throw it out
	adc_read_conversion_data(dev_num, 0);
	if(mio_error_code)
		return 1;

	// Finish the rest of the channels
	for(x=1; x<8; x++)
	{
		adc_start_conversion(dev_num, x);
		if(mio_error_code)
			return 1;

		adc_wait_ready(dev_num, x);
		if(mio_error_code)
			return 1;

		// Store the results in the user's buffer
		adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, x);
		if(mio_error_code)
			return 1;
	}

	// A final dummy conversion is required to get out the last data
	adc_start_conversion(dev_num, 7);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, 7);
	if(mio_error_code)
		return 1;

	adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, 7);
	if(mio_error_code)
		return 1;

	// Now start on the second controller
	adc_start_conversion(dev_num, 8);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, 8);
	if(mio_error_code)
		return 1;

	// This data is old - Throw it out
	adc_read_conversion_data(dev_num, 8);
	if(mio_error_code)
		return 1;

	for(x=9; x<16; x++)
	{
		adc_start_conversion(dev_num, x);
		if(mio_error_code)
			return 1;

		adc_wait_ready(dev_num, x);
		if(mio_error_code)
			return 1;

		adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, x);
		if(mio_error_code)
			return 1;
	}

	// A final dummy conversion is required to get the last data
	adc_start_conversion(dev_num, 15);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, 15);
	if(mio_error_code)
		return 1;

	adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, 15);
	if(mio_error_code)
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// adc_convert_to_volts
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			value		Value to be converted to volts
//
// Returns:
//			VAL	Voltage conversion
//			0	Invalid channel
//
//------------------------------------------------------------------------
float adc_convert_to_volts(int dev_num, int channel, unsigned short value)
{
	float result;

		if((channel < 0) || (channel > 15))
			return (0.0);

		value = value + adc_adjust[dev_num][channel];
		result = value * adc_bitval[dev_num][channel];
		result = result + adc_offset[dev_num][channel];
		return result;
}

//------------------------------------------------------------------------
//
// adc_convert_single_repeated
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			count		Number of conversions
//			buffer		Storage of channel data
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int adc_convert_single_repeated(int dev_num, int channel, unsigned short count, unsigned short *buffer)
{
	int x;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Setup global variables including transferring the address of the
	// user's output buffer to a global variable the ISR knows about.
	adc_user_buffer = buffer;
	adc_out_index[dev_num] = 0;
	adc_repeat_channel[dev_num] = channel;
	adc_repeat_count[dev_num] = count;

	adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	// This data is old, we don't want it
	adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	// Perform the requested number of conversions. Place the results into
	// the user's buffer.
	for(x=0; x<=adc_repeat_count[dev_num]; x++)
	{
		adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);
		if(mio_error_code)
			return 1;

		adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);
		if(mio_error_code)
			return 1;

		adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);
		if(mio_error_code)
			return 1;
	}

	// One last dummy conversion to retrieve our last data
	adc_start_conversion(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_repeat_channel[dev_num]);
	if(mio_error_code)
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// adc_buffered_channel_conversions
//
// Arguments:
//			dev_num		The index of the chip
//			input_channel_buffer
//			buffer		Storage of channel data
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int adc_buffered_channel_conversions(int dev_num, unsigned char *input_channel_buffer, unsigned short *buffer)
{
	int adc_next_channel;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	adc_ch_index[dev_num] = 0;
	adc_out_index[dev_num] = 0;
	
	adc_user_buffer = buffer;
	adc_input_buffer = input_channel_buffer;

	// Reset all of the array index pointers

	adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);

	if(mio_error_code)
		return 1;

	adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);

	if(mio_error_code)
		return 1;

	// While there are channel numbers in the buffer (1= 0xff)
	// convert the requested channel and place the result in the
	// user's output buffer
	while(adc_input_buffer[adc_ch_index[dev_num]] != 0xff)
	{
		adc_next_channel = adc_input_buffer[adc_ch_index[dev_num]];

		// This function is particularly tricky because of the
		// fact that the data is delayed by one conversion and if
		// we switch back and forth between the two controllers
		// we'll need to run an extra conversion in order to get the
		// last data offering from the previous controller. The
		// conditional code in the next several lines handles the
		// switches from one controller to the other.  
		if(adc_current_channel[dev_num] < 8 && adc_next_channel > 7)
		{
			adc_start_conversion(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_wait_ready(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);
			if(mio_error_code)
				return 1;

			adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);
			if(mio_error_code)
				return 1;
		}
		else if(adc_current_channel[dev_num] > 7 && adc_next_channel < 8)
		{
			adc_start_conversion(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_wait_ready(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);
			if(mio_error_code)
				return 1;

			adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);
			if(mio_error_code)
				return 1;

			adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);
			if(mio_error_code)
				return 1;
		}
		adc_start_conversion(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);
		if(mio_error_code)
			return 1;

		adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]++]);
		if(mio_error_code)
			return 1;

		adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_current_channel[dev_num]);
		if(mio_error_code)
			return 1;
	}

	// One last conversion allows us to retrieve our real last data
	adc_start_conversion(dev_num, adc_input_buffer[--adc_ch_index[dev_num]]);
	if(mio_error_code)
		return 1;
	adc_wait_ready(dev_num, adc_input_buffer[adc_ch_index[dev_num]]);
	if(mio_error_code)
		return 1;

	adc_user_buffer[adc_out_index[dev_num]++] = adc_read_conversion_data(dev_num, adc_last_channel[dev_num]);
	if(mio_error_code)
		return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// adc_wait_ready
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			0	The function completes successfully
//			1	Channel time-out
//
//------------------------------------------------------------------------
int adc_wait_ready(int dev_num, int channel)
{
	long retry;
	
	mio_error_code = MIO_SUCCESS;
	retry = 100000l;

	// Like with the DAC timeout routine, under normal circumstances we'll
	// barely make it through the loop one time beacuse the hadrware is plenty
	// fast. We have the delay for the rare occasion and when the hadrware is not
	// responding properly.  
	while(retry--)
	{
		if(adc_read_status(dev_num, channel / 8) & 0x80)
			return 0;
	}

	mio_error_code = MIO_TIMEOUT_ERROR;
	sprintf(mio_error_string,"MIO(ADC) : Wait ready - Device timeout error\n");

	return 1;
}

//------------------------------------------------------------------------
//
// write_adc_command
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			value		ADC command
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int write_adc_command(int dev_num, int adc_num, unsigned char value)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

    ioctl(handle[dev_num], WRITE_ADC_COMMAND, (value << 8) | adc_num);

	return 0;
}

//------------------------------------------------------------------------
//
// buffered_dac_output
//
// Arguments:
//			dev_num		The index of the chip
//			cmd_buff	Command list
//			data_buff	Data list
//
// Returns:
//			0	The function completes successfully
//			1	Error
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int buffered_dac_output(int dev_num, unsigned char *cmd_buff, unsigned short *data_buff)
{
	int x= 0;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	while(1)
	{
		if(cmd_buff[x] == 0xff)
			return 0;

		if(set_dac_output(dev_num, cmd_buff[x], data_buff[x]))
			return 1;

		x++;
	}
}

//------------------------------------------------------------------------
//
// write_dac_data
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//			value		New DAC value
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int write_dac_data(int dev_num, int dac_num, unsigned short value)
{
	int ret_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

    ret_val = ioctl(handle[dev_num], WRITE_DAC_DATA, (value << 8) | dac_num);
	
	return 0;
}

//------------------------------------------------------------------------
//
// dac_read_status
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Returns:
//			VAL	DAC status value
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
unsigned char dac_read_status(int dev_num, int dac_num)
{
	int ret_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

    ret_val = ioctl(handle[dev_num], READ_DAC_STATUS, dac_num);

	return (ret_val & 0xff);
}

//------------------------------------------------------------------------
//
// adc_read_status
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Returns:
//			VAL	ADC status value
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
unsigned char adc_read_status(int dev_num, int adc_num)
{
	int ret_val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

    ret_val = ioctl(handle[dev_num], READ_ADC_STATUS, adc_num);

	return (ret_val & 0xff);
}

//------------------------------------------------------------------------
//
// adc_set_channel_mode
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//			input_mode	Desired channel mode
//			duplex		Desired channel duplex
//			range		Desired channel range
//
// Returns:
//			0	The function completes successfully
//			1	Error, check error code
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int adc_set_channel_mode(int dev_num, int channel, int input_mode, int duplex, int range)
{
	unsigned char command_byte;

	mio_error_code = MIO_SUCCESS;

	if(dev_num < 0 || dev_num > MAX_DEV)
	{
		mio_error_code = MIO_BAD_DEVICE;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Device Number %d\n",dev_num);
		return 1;
	}

	if(channel < 0 || channel > 15)
	{
		mio_error_code = MIO_BAD_CHANNEL_NUMBER;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Channel Number %d\n",channel);
		return 1;
	}

	// Check for illegal modes
	if((input_mode != ADC_SINGLE_ENDED) && (input_mode != ADC_DIFFERENTIAL))
	{
		mio_error_code = MIO_BAD_MODE_NUMBER;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Mode Number\n");
		return 1;
	}

	if((duplex != ADC_UNIPOLAR) && (duplex != ADC_BIPOLAR))
	{
		mio_error_code = MIO_BAD_MODE_NUMBER;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Mode Number\n");
		return 1;
	}

	if((range != ADC_TOP_5V) && (range != ADC_TOP_10V))
	{
		mio_error_code = MIO_BAD_RANGE;
		sprintf(mio_error_string,"MIO(ADC) : Set Channel Mode - Bad Range Value\n");
		return 1;
	}

	command_byte = adc_channel_select[dev_num][channel];
	command_byte = command_byte | input_mode | duplex | range;

	// Building these four arrays at mode set time is critical for speed
	// as we don't need to calculate anything when we want to start an ADC
	// conversion. WE simply retrieve the command byte from the array
	// and send it to the controller.
	// Likewise, when doing conversion from raw 16-bit values to a voltage
	// the mode controls the worth of each individual bit as well as binary
	// bias and offset values.  
	adc_channel_mode[dev_num][channel] = command_byte;

	// Calculate bit values, offset, and adjustment values  
	if((range == ADC_TOP_5V) && (duplex == ADC_UNIPOLAR))
	{
		adc_bitval[dev_num][channel] = 5.00 / 65536.0;
		adc_adjust[dev_num][channel] = 0;
		adc_offset[dev_num][channel] = 0.0;
	}

	if((range == ADC_TOP_5V) && (duplex == ADC_BIPOLAR))
	{
		adc_bitval[dev_num][channel] = 10.0 / 65536.0;
		adc_adjust[dev_num][channel] = 0x8000;
		adc_offset[dev_num][channel] = -5.000;
	}

	if((range == ADC_TOP_10V) && (duplex == ADC_UNIPOLAR))
	{
		adc_bitval[dev_num][channel] = 10.0 / 65536.0;
		adc_adjust[dev_num][channel] = 0;
		adc_offset[dev_num][channel] = 0.0;
	}

	if((range == ADC_TOP_10V) && (duplex == ADC_BIPOLAR))
	{
		adc_bitval[dev_num][channel] = 20.0 / 65536.0;
		adc_adjust[dev_num][channel] = 0x8000;
		adc_offset[dev_num][channel] = -10.0;
	}

	return 0;
}

//------------------------------------------------------------------------
//
// adc_read_conversion_data
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			VAL	ADC channel voltage
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
unsigned short adc_read_conversion_data(int dev_num, int channel)
{
	int ret_val;
	int adc_num;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	if(channel > 7)
		adc_num = 1;
	else
		adc_num = 0;

    ret_val = ioctl(handle[dev_num], READ_ADC_DATA, adc_num);
	
	return (ret_val & 0xffff);
}

//------------------------------------------------------------------------
//
// adc_auto_get_channel_voltage
//
// Arguments:
//			dev_num		The index of the chip
//			channel		ADC channel
//
// Returns:
//			VAL	ADC channel voltage
//			0	Error
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
float adc_auto_get_channel_voltage(int dev_num, int channel)
{
	unsigned short value;
	float result;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Start out on a +/-10 Volt scale
	adc_set_channel_mode(dev_num, channel,ADC_SINGLE_ENDED,ADC_BIPOLAR,ADC_TOP_10V);
	if(mio_error_code)
		return (0.0);

	adc_start_conversion(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_start_conversion(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	value = adc_read_conversion_data(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	// Convert the raw data to voltage
	value = value + adc_adjust[dev_num][channel];
	result = value * adc_bitval[dev_num][channel];
	result = result + adc_offset[dev_num][channel];

	#ifdef DEBUG
		printf("auto_get_channel_voltage : Raw = %04x, adjust = %d, bitval = %9.5f, offset = %9.5f,  result = %9.5f\n",
		        value - adc_adjust[dev_num][channel],adc_adjust[dev_num][channel],adc_bitval[dev_num][channel],adc_offset[dev_num][channel],result);
	#endif

	// If the voltage is less than -5.00 volts, we're as precise as we can get
	if(result <= -5.00)
		return result;

	// If the result is between -4.99 and 0.0 we can  to the +/- 5V scale.
	if(result < 0.0)
		adc_set_channel_mode(dev_num, channel,ADC_SINGLE_ENDED,ADC_BIPOLAR,ADC_TOP_5V);

	if(mio_error_code)
		return (0.0);

	// If the result is above 5 volts a 0 - 10V range will work best
	if(result >= 5.00)
		adc_set_channel_mode(dev_num, channel,ADC_SINGLE_ENDED,ADC_UNIPOLAR,ADC_TOP_10V);

	if(mio_error_code)
		return (0.0);

	// Lastly if we're greater than 0 and less than 5 volts the 0-5V scale is best
	if((result >= 0.0) && (result < 5.00))
		adc_set_channel_mode(dev_num, channel, ADC_SINGLE_ENDED, ADC_UNIPOLAR,ADC_TOP_5V);

	if(mio_error_code)
		return (0.0);

	// Now that the values is properly ranged, we take two more samples
	// to get a current reading at the new scale.
	adc_start_conversion(dev_num, channel);

	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);

	if(mio_error_code)
		return (0.0);

	adc_start_conversion(dev_num, channel);

	if(mio_error_code)
		return (0.0);

	adc_wait_ready(dev_num, channel);

	if(mio_error_code)
		return (0.0);

	value = adc_read_conversion_data(dev_num, channel);
	if(mio_error_code)
		return (0.0);

	// Convert the raw data to voltage
	value = value + adc_adjust[dev_num][channel];
	result = value * adc_bitval[dev_num][channel];
	result = result + adc_offset[dev_num][channel];

	#ifdef DEBUG
		printf("auto_get_channel_voltage : Raw = %04x, adjust = %d, bitval = %9.5f, offset = %9.5f,  result = %9.5f\n",
		        value - adc_adjust[dev_num][channel],adc_adjust[dev_num][channel],adc_bitval[dev_num][channel],adc_offset[dev_num][channel],result);
	#endif

	return result;
}

//------------------------------------------------------------------------
//
// dio_read_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to read
//
// Returns:
//			0	Bit is clear
//			1	Bit is set
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_read_bit(int dev_num, int bit_number)
{
	unsigned char port;
	int val;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Adjust for 0 - 47 bit numbering
	--bit_number;

	port = bit_number / 8;

	val = read_dio_byte(dev_num, port);

	// Get just the bit we specified
	val = val & (1 << (bit_number % 8));

	// adjust the return for a 0 or 1 value
	if(val) return 1;

	return 0;
}

//------------------------------------------------------------------------
//
// dio_write_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to write
//			val			New value of bit
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_write_bit(int dev_num, int bit_number, int val)
{
	unsigned char port;
	unsigned char temp;
	unsigned char mask;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Adjust bit numbering for 0 based numbering
	--bit_number;

	// Calculate the address of the port based on bit number
	port = bit_number / 8;

	// Use the image value to avoid having to read from the port first
	temp = dio_port_images[dev_num][bit_number / 8];

	// Calculate the bit mask for the specifed bit
	mask = (1 << (bit_number %8));

	// Check whether the request was to set or clear the bit
	if(val)
		temp = temp | mask;
	else
		temp = temp & ~mask;

	// Update the image value with the value we're about to write
	// PBD 8-29-11 Moved to write_dio_byte
	//dio_port_images[dev_num][port] = temp;

	write_dio_byte(dev_num, port, temp);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_set_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to set
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_set_bit(int dev_num, int bit_number)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	dio_write_bit(dev_num, bit_number,1);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_clr_bit
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_clr_bit(int dev_num, int bit_number)
{
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	dio_write_bit(dev_num, bit_number,0);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_enab_bit_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//			polarity	Rising or falling edge
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_enab_bit_int(int dev_num, int bit_number, int polarity)
{
	unsigned char port;
	unsigned char temp;
	unsigned char mask;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Adjust the bit number for 0 based numbering
	--bit_number;

	// Calculate the offset for the enable port
	port = (bit_number / 8) + 8;

	// Calculate the proper bit mask for this bit number
	mask = (1 << (bit_number % 8));

	// Turn on access to page 2 registers
	write_dio_byte(dev_num, 0x07,0x80);

	// Get the current state of the enable register
	temp = read_dio_byte(dev_num, port);

	// Set the enable bit for our bit number
	temp = temp | mask;

	// Now update the interrupt enable register
	write_dio_byte(dev_num, port, temp);

	// Turn on access to page 1 for polarity control
	write_dio_byte(dev_num, 0x07,0x40);

	temp = read_dio_byte(dev_num, port);

	// Set the polarity according to the argument value
	if(polarity)
		temp = temp | mask;
	else
		temp = temp & ~mask;

	write_dio_byte(dev_num, port, temp);

	// Set access back to page 0
	write_dio_byte(dev_num, 0x07,0);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_disab_bit_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_disab_bit_int(int dev_num, int bit_number)
{
	unsigned char port;
	unsigned char temp;
	unsigned char mask;

	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	// Adjust the bit number for 0 based numbering
	--bit_number;

	// Calculate the offset for the enable port
	port = (bit_number / 8) + 8;

	// Calculate the proper bit mask for this bit number
	mask = (1 << (bit_number % 8));

	// Turn on access to page 2 registers
	write_dio_byte(dev_num, 0x07,0x80);

	// Get the current state of the enable register
	temp = read_dio_byte(dev_num, port);

	// Clear the enable bit for the our bit
	temp = temp & ~mask;

	// Update the enable register with the new data
	write_dio_byte(dev_num, port,temp);

	// Set access back to page 0
	write_dio_byte(dev_num, 0x07,0);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_clr_int
//
// Arguments:
//			dev_num		The index of the chip
//			bit_number	Bit to clear
//
// Returns:
//			0	The function completes successfully
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_clr_int(int dev_num, int bit_number)
{
	unsigned short port;
	unsigned short temp;
	unsigned short mask;

	// Adjust for 0 based numbering
	mio_error_code = MIO_SUCCESS;

	if(check_handle(dev_num))   // Check for chip available  
		return -1;

	--bit_number;

	// Calculate the correct offset for our enable register
	port = (bit_number / 8) + 8;

	// Calculate the bit mask for this bit
	mask = (1 << (bit_number % 8));

	// Set access to page 2 for the enable register
	write_dio_byte(dev_num, 0x07,0x80);

	// Get the current state of the register
	temp = read_dio_byte(dev_num, port);

	// Temporarily clear only our enable. This clears the interrupt
	temp = temp & ~mask;

	// Write out the temporary value
	write_dio_byte(dev_num, port, temp);

	temp = temp | mask;

	write_dio_byte(dev_num, port, temp);

	// Set access back to page 0
	write_dio_byte(dev_num, 0x07,0);

	return 0;
}

//------------------------------------------------------------------------
//
// dio_get_int
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			IRQ	Interrupt
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int dio_get_int(int dev_num)
{
	int val;

    val = ioctl(handle[dev_num], DIO_GET_INT, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// wait_adc_int
//
// Arguments:
//			dev_num		The index of the chip
//			adc_num		ADC device number
//
// Returns:
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int wait_adc_int(int dev_num, int adc_num)
{
	int val;

    if(check_handle(dev_num))   // Check for chip available  
		return -1;

	if(adc_num)
	    val = ioctl(handle[dev_num], WAIT_ADC_INT_2, NULL);
	else
	    val = ioctl(handle[dev_num], WAIT_ADC_INT_1, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// wait_dac_int
//
// Arguments:
//			dev_num		The index of the chip
//			dac_num		DAC device number
//
// Returns:
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int wait_dac_int(int dev_num, int dac_num)
{
	int val;

    if(check_handle(dev_num))   // Check for chip available  
		return -1;

	if(dac_num)
	    val = ioctl(handle[dev_num], WAIT_DAC_INT_2, NULL);
	else
	    val = ioctl(handle[dev_num], WAIT_DAC_INT_1, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// wait_dio_int
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			-1	The chip does not exist or it's handle is invalid
//
//------------------------------------------------------------------------
int wait_dio_int(int dev_num)
{
	int val;

    if(check_handle(dev_num))   // Check for chip available  
		return -1;

    val = ioctl(handle[dev_num], WAIT_DIO_INT, NULL);

    return (val & 0xff);
}

//------------------------------------------------------------------------
//
// check_handle
//
// Arguments:
//			dev_num		The index of the chip
//
// Returns:
//			0	The function completes successfully
//			-1	Error, check error code
//
//------------------------------------------------------------------------
int check_handle(int dev_num)
{
	if(handle[dev_num] > 0)	// If it's already a valid handle  
		return 0;

    if(handle[dev_num] == -1)	// If it's already been tried  
	{
		mio_error_code = MIO_OPEN_ERROR;
		sprintf(mio_error_string,"MIO - Unable to open device PCMMIO\n");
		return -1;
	}

    // Try opening the device file, in case it hasn't been opened yet  
    handle[dev_num] = open(device_id[dev_num], O_RDWR);

    if(handle[dev_num] > 0)	// If it's now a valid handle  
		return 0;

	mio_error_code = MIO_OPEN_ERROR;
	sprintf(mio_error_string,"MIO - Unable to open device PCMMIO\n");
    handle[dev_num] = -1;
	return -1;
}
