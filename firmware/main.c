/*
 * ATtiny85 used to measure voltage of a lithium battery
 * Flash the whole volts and the first decimal (100mV) upon start then measure every 4s and compare to last value blinked
 * If voltage dropped by 100mV then blink the voltage again
 * If voltage dropped below 3V then give one flash every time a measurement is done
 *
 * Created: 20/03/2023 19:27:53
 * Author : fesix
 *
 * LED output: PB4
 * Battery voltage input: PB3 (ADC 3)
 * 
 */ 

#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define LED_PORT	PB4
#define ADCfactor	0.005

float voltage;
float voltage_last;
int volts_whole;
int volts_decimal;

// Initialize ADC
void ADC_init(void)
{
	uint16_t result;
	
	ADMUX |= (1 << REFS2) | (1 << REFS1);					// Internal 2.56V reference without ext. cap
	ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);	// 128 ADC clock prescaler
	ADCSRA |= (1 << ADEN);									// ADC enable
	
	//Dummy-Readout
	ADCSRA |= (1 << ADSC);									// ADC single conversion
	while (ADCSRA & (1 << ADSC)) {}							// Wait until conversion is finished
	
	result = ADCW;											// Read register of result and do nothing with it
}

// Read ADC value
uint16_t ADC_Read(uint8_t channel)
{
	ADMUX = (ADMUX & ~(0x0F)) | (channel & 0x0F);			// Select ADC channel without interfering with other bits in register
	ADCSRA |= (1 << ADSC);									// ADC single conversion
	while (ADCSRA & (1 << ADSC)) {}							// Wait until conversion is finished
	
	return ADCW;											// Return value
}

// Take multiple samples and average them
uint16_t ADC_Avg(uint8_t channel, uint8_t average)
{
	uint16_t adcsum = 0;
	
	for (int i = 0; i < average; i++)						// Read ADC a given number of times
	{
		adcsum += ADC_Read(channel);						// Add up all values
	}
	
	return adcsum / average;								// Divide sum by number of samples
}

//  Flash LED a given number of times
void flash_LED (uint8_t number)
{
	for (int i = 0; i < number; i++)
	{
		PORTB |= (1 << LED_PORT);							// LED on
		_delay_ms(100);
		PORTB &= ~(1 << LED_PORT);							// LED off
		_delay_ms(200);
	}
}


int main(void)
{
    DDRB |= (1 << LED_PORT);								// Output
	
	ADC_init();
	
	// Indicate charge after turning on
	voltage = ADC_Avg(3, 10) * ADCfactor;					// Read ADC and calculate battery voltage
	voltage_last = voltage;									// store last blinked value
	_delay_ms(1000);										// Wait a bit
	volts_whole = (int) voltage;							// Convert to whole volts
	flash_LED(volts_whole);									// Flash whole volts
	_delay_ms(1000);										// Wait a bit
	volts_decimal = (voltage * 10.0) - (volts_whole * 10.0);	// Calculate just the first decimal as int
	flash_LED((int) volts_decimal);							// Flash decimal

	// Timer configuration, as slow as possible (4s)
	TCCR1 |= (1 << CTC1) | (1 << CS13) | (1 << CS12) | (1 << CS11) | (1 << CS10);	// CTC mode, prescaler 16384
	OCR1C =	255;											// Compare value
	TIMSK |= (1 << OCIE1A);									// Interrupt enable
	
	sei();													// Enable global interrupts
	
	while(1)												// Do nothing, ISR does everything
	{}
}

ISR(TIMER1_COMPA_vect)
{
	voltage = ADC_Avg(3, 10) * ADCfactor;					// Read ADC and calculate battery voltage
	
	if (voltage < 3.0)										// If voltage dropped to less than 3V
	{
		flash_LED(1);										// Flash LED once every measurement
		_delay_ms(500);
	}
	
	if ((voltage_last-voltage) > 0.1)						// If voltage dropped by more than 100mV since last blinked value 
	{
			volts_whole = (int) voltage;							// Convert to whole volts
			flash_LED(volts_whole);									// Flash whole volts
			_delay_ms(1000);										// Wait a bit
			volts_decimal = (voltage * 10.0) - (volts_whole * 10.0);	// Calculate just the first decimal as int
			flash_LED((int) volts_decimal);							// Flash decimal
			
			voltage_last = voltage;									// Store last blinked value
	}
}