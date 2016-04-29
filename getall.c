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
//	Name	 : getall.c
//
//	Project	 : PCMMIO Sample Application Program
//
//	Author	 : Paul DeMetrotion
//
//****************************************************************************
//
//	  Date		Revision	                Description
//	--------	--------	---------------------------------------------
//	11/11/10	  1.0		Original Release	
//	10/09/12	  3.0		Cleaned up	
//
//****************************************************************************

#include "mio_io.h" // Our IOCTL definitions and all function prototypes    
#include <stdio.h>
#include <stdlib.h>

// This program demonstrates usage of the adc_convert_all_channels function call.
// This allows for all 16 channels to be quickly converted and the results passed
// back in an 16 element array. Note that this function does not return voltage values,
// it returns raw 16-bit data directly from the converter.

// This array will receive the result values for all 16 channels
unsigned short values[16];

int main(int argc, char *argv[])
{
	int dev, channel;
	unsigned short result;
	float current;

	if (argc !=2)
	{
		printf("\nUsage: getall <devnum>\n");
		printf("  getall 1\n");
		exit(1);
	}

	dev = atoi(argv[1]);

	// We set the mode on all 16 channels to single-ended bipolar +/- 10V scale.
	// This allows for any device legal input voltages
	for(channel=0; channel < 16; channel++)
	{			
		adc_set_channel_mode(dev,channel,ADC_SINGLE_ENDED,ADC_BIPOLAR,ADC_TOP_10V);
			
		// Check for an error by loooking at mio_error_code
		if(mio_error_code)
		{
			// If an error occurs, print out the string and exit
			printf("%s - Aborting\n",mio_error_string);
			exit(1);
		}
	}
		
	// This is it! When this function returns the values from all 8 channels
	// will be present in the values array
	adc_convert_all_channels(dev,values);
			
	// Check for possible errors
	if(mio_error_code)
	{
		printf("%s - Aborting\n",mio_error_string);
		exit(1);
	}	
			
	// Now we'll extract the data, convert it to volts, and display the results
	for(channel =0; channel <16; channel++)
	{
		// This is for print formatting
		if(channel == 4 || channel == 8 || channel == 12)
			printf("\n");
			
		// Get a result from the array
		result = values[channel];
			
		// Convert the raw value to voltage
		current = adc_convert_to_volts(dev,channel,result);

		// Display the result
		printf("CH%2d%8.4f | ",channel,current);
	}
		
	printf("\n\n");
}
