#define EXT_TC_ONE_WIRE  // Use MAX31850K One Wire Interface
//#define EXT_TC_SPI     // Use MAX31855 /w I2C to SPI bridge

#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "t962.h"
#ifdef EXT_TC_ONE_WIRE
	#include "onewire.h"
#endif
#ifdef EXT_TC_SPI
	#include "max31855.h"
#endif
#include "nvstorage.h"

#include "sensor.h"

// Gain adjust, this may have to be calibrated per device if factory trimmer adjustments are off
float adcgainadj[2];
 // Offset adjust, this will definitely have to be calibrated per device
float adcoffsetadj[2];

float temperature[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
uint8_t tempvalid = 0;
uint8_t cjsensorpresent = 0;

// The feedback temperature
float avgtemp;
float inner_avgtemp;
float coldjunction;
uint8_t TCfeedback;

void Sensor_DoConversion(void) {
	uint16_t temp[2];
	/*
	* These are the temperature readings we get from the thermocouple interfaces
	* Right now it is assumed that if they are indeed present the first two
	* channels will be used as feedback. If TC Extra 1 and/or TC Extra 2 are present, 
	* they will be used as feedback variable instead. 
	*/
	float tctemp[4], tccj[4];
	uint8_t tcpresent[4];
	tempvalid = 0; // Assume no valid readings;
#ifdef EXT_TC_ONE_WIRE
	for (int i = 0; i < 4; i++) { // Get 4 TC channels
		tcpresent[i] = OneWire_IsTCPresent(i);
		if (tcpresent[i]) {
			tctemp[i] = OneWire_GetTCReading(i);
			tccj[i] = OneWire_GetTCColdReading(i);
			if (i > 1) {
				temperature[i] = tctemp[i];
				tempvalid |= (1 << i);
			}
		}
	}
#endif
#ifdef EXT_TC_SPI
	for (int i = 0; i < 4; i++) { // Get 4 TC channels
		tcpresent[i] = SPI_IsTCPresent(i);
		if (tcpresent[i]) {
			tctemp[i] = SPI_GetTCReading(i);
			tccj[i] = SPI_GetTCColdReading(i);
			if (i > 1) {
				temperature[i] = tctemp[i];
				tempvalid |= (1 << i);
			}
		}
	}
#endif

	// Assume no CJ sensor
	cjsensorpresent = 0;

	// We have attached one or two additional TCs, use their reading as feedback.
	if ((tcpresent[0] && tcpresent[1]) && (tcpresent[2] || tcpresent[3]) && NV_GetConfig(REFLOW_USE_EXT_TC)) {  
		if (tcpresent[2] && tcpresent[3]) {
			avgtemp = (tctemp[2] + tctemp[3]) / 2.0f;
			coldjunction = (tccj[0] + tccj[1] + tccj[2] + tccj[3]) / 4.0f;
			TCfeedback = 0x03;
			}
		else if (tcpresent[2]) {
			avgtemp = tctemp[2];
			coldjunction = (tccj[0] + tccj[1] + tccj[2]) / 3.0f;
			TCfeedback = 0x02;			
		}
		else if (tcpresent[3]) {
			avgtemp = tctemp[3];
			coldjunction = (tccj[0] + tccj[1] + tccj[3]) / 3.0f;	
			TCfeedback = 0x01;		
		}
		temperature[0] = tctemp[0];
		temperature[1] = tctemp[1];
		inner_avgtemp = temperature[0] + temperature[1] / 2.0f;
		tempvalid |= 0x03;
		cjsensorpresent = 1;
	}
	// We only have the built-in TCs
	else if (tcpresent[0] && tcpresent[1]) {  
		avgtemp = (tctemp[0] + tctemp[1]) / 2.0f;
		temperature[0] = tctemp[0];
		temperature[1] = tctemp[1];
		inner_avgtemp = temperature[0] + temperature[1] / 2.0f;
		tempvalid |= 0x03;
		coldjunction = (tccj[0] + tccj[1]) / 2.0f;
		TCfeedback = 0x0c;
		cjsensorpresent = 1;
	}
	else {
		// If the external TC interface is not present we fall back to the
		// built-in ADC, with or without compensation
		coldjunction = OneWire_GetTempSensorReading();
		if (coldjunction < 127.0f) {
			cjsensorpresent = 1;
		} else {
			coldjunction = 25.0f; // Assume 25C ambient if not found
		}
		temp[0] = ADC_Read(1);
		temp[1] = ADC_Read(2);

		// ADC oversamples to supply 4 additional bits of resolution
		temperature[0] = ((float)temp[0]) / 16.0f;
		temperature[1] = ((float)temp[1]) / 16.0f;

		// Gain adjust
		temperature[0] *= 1.0f;
		temperature[1] *= 1.0f;

		// Offset adjust
		temperature[0] += coldjunction + 0.0f;
		temperature[1] += coldjunction + 0.0f;

		tempvalid |= 0x03;

		avgtemp = (temperature[0] + temperature[1]) / 2.0f;
		TCfeedback = 0x0c;
		inner_avgtemp = temperature[0] + temperature[1] / 2.0f;
	}
}

uint8_t Sensor_ColdjunctionPresent(void) {
	return cjsensorpresent;
}


float Sensor_GetTemp(TempSensor_t sensor) {
	if (sensor == TC_COLD_JUNCTION) {
		return coldjunction;
	} else if(sensor == TC_AVERAGE) {
		return avgtemp;
	} else if(sensor == TC_INNER_AVERAGE) {
		return inner_avgtemp;
	} else if(sensor < TC_NUM_ITEMS) {
		return temperature[sensor - TC_LEFT];
	} else {
		return 0.0f;
	}
}

uint8_t Sensor_GetFeedbackTC(void) {
	return TCfeedback;
}

uint8_t Sensor_IsValid(TempSensor_t sensor) {
	if (sensor == TC_COLD_JUNCTION) {
		return cjsensorpresent;
	} else if(sensor == TC_AVERAGE) {
		return 1;
	} else if(sensor >= TC_NUM_ITEMS) {
		return 0;
	}
	return (tempvalid & (1 << (sensor - TC_LEFT))) ? 1 : 0;
}

void Sensor_ListAll(void) {
	int count = 5;
	char* names[] = {"Left", "Right", "Extra 1", "Extra 2", "Cold junction"};
	TempSensor_t sensors[] = {TC_LEFT, TC_RIGHT, TC_EXTRA1, TC_EXTRA2, TC_COLD_JUNCTION};
	char* format = "\n%13s: %4.1fdegC";

	for (int i = 0; i < count; i++) {
		if (Sensor_IsValid(sensors[i])) {
			printf(format, names[i], Sensor_GetTemp(sensors[i]));
		}
	}
	if (!Sensor_IsValid(TC_COLD_JUNCTION)) {
		printf("\nNo cold-junction sensor on PCB");
	}
}
