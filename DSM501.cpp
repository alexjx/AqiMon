#include "DSM501.h"
#include <avr/interrupt.h>

DSM501 DSM501::_instance;

/*
 * These PINs are hard coded to cope with interrupt
 * 	PM10 -> ADC3/PCINT11
 * 	PM25 -> ADC2/PCINT10
 */
const int DSM501::_pin[] = { PM10_PIN, PM25_PIN };

void DSM501::Isr() {
	DSM501* sensor = DSM501::getInst();
	sensor->_pinState[0] = digitalRead(sensor->_pin[0]);
	sensor->_pinState[1] = digitalRead(sensor->_pin[1]);
}

ISR(PCINT1_vect) {
	DSM501::Isr();
}


DSM501::DSM501() {
	for (int i = 0; i < 2; i++) {
		_tm_ms_start[i] = 0;
		_tm_us_total[i] = 0;
		_state[i] = S_Idle;
		_pinState[i] = HIGH;
		_tm_us_sig_start[i] = 0;
		_lastLowRatio[i] = NAN;
	}
}

void DSM501::begin() {
	_tm_ms_start[0] = millis();
	pinMode(_pin[0], INPUT);

	_tm_ms_start[1] = millis();
	pinMode(_pin[1], INPUT);

	cli();
	PCICR 	|= 	_BV(1);
	PCMSK1 	|=  _BV(2) | _BV(3);
	sei();
}

void DSM501::update() {
	if (_state[0] == S_Idle && _pinState[0] == LOW) {
		signal_begin(0);
		_state[0] = S_Start;
	} else if (_state[0] == S_Start && _pinState[0] == HIGH) {
		signal_end(0);
		_state[0] = S_Idle;
	}

	if (_state[1] == S_Idle && _pinState[1] == LOW) {
		signal_begin(1);
		_state[1] = S_Start;
	} else if (_state[1] == S_Start && _pinState[1] == HIGH) {
		signal_end(1);
		_state[1] = S_Idle;
	}
}


void DSM501::signal_begin(int i) {
	_tm_us_sig_start[i] = micros();
}


void DSM501::signal_end(int i) {
	ulong_t tm_us_now = micros();
	if (_tm_us_sig_start[i] <= tm_us_now && 						// we had a signal, and
		(tm_us_now - _tm_us_sig_start[i]) >= DSM501_MIN_SIG_SPAN) 	// this signal is not bouncing.
	{
		_tm_us_total[i] += tm_us_now - _tm_us_sig_start[i];
		_tm_us_sig_start[i] = 0;
	}
}

/*
 * Only return the stablized ratio
 */
double DSM501::getLowRatio(int i) {
	ulong_t tm_ms_now = millis();
	ulong_t tm_ms_span = tm_ms_now - _tm_ms_start[i];

	if (tm_ms_span >= DSM501_MIN_RATIO_SPAN) {
		_tm_ms_start[i] = tm_ms_now;
		double tm_ms_total_low = (double)_tm_us_total[i] / 10.0;
		_tm_us_total[i] = 0;

		_lastLowRatio[i] = tm_ms_total_low / (double)tm_ms_span;
	}

	return _lastLowRatio[i];
}


ulong_t DSM501::getParticalPcs(int i) {
	return (ulong_t)(getLowRatio(i) / 100.0 * 4.5 * 12500.0);
}


double DSM501::getParticalWeight(int i) {
	/*
	 * with data sheet... upper boundary around 15% is 1.4mg/m3
	 */
	return getLowRatio(i) / MAX_LOW_RATIO_OF_1400ug * 1400.0;
}

int DSM501::getAQI() {
	// this works only under both pin configure
	int aqi = -1;

	double P25Weight = getParticalWeight(0) - getParticalWeight(1);
	if (P25Weight <= 15.4) {
		aqi = 0   +(int)(50.0 / 15.5 * P25Weight);
	} else if (P25Weight > 15.5 && P25Weight <= 40.5) {
		aqi = 50  + (int)(50.0 / 25.0 * (P25Weight - 15.5));
	} else if (P25Weight > 40.5 && P25Weight <= 65.5) {
		aqi = 100 + (int)(50.0 / 25.0 * (P25Weight - 40.5));
	} else if (P25Weight > 65.5 && P25Weight <= 150.5) {
		aqi = 150 + (int)(50.0 / 85.0 * (P25Weight - 65.5));
	} else if (P25Weight > 150.5 && P25Weight <= 250.5) {
		aqi = 200 + (int)(100.0 / 100.0 * (P25Weight - 150.5));
	} else if (P25Weight > 250.5 && P25Weight <= 350.5) {
		aqi = 300 + (int)(100.0 / 100.0 * (P25Weight - 250.5));
	} else if (P25Weight > 350.5 && P25Weight <= 500.0) {
		aqi = 400 + (int)(100.0 / 150.0 * (P25Weight - 350.5));
	} else if (P25Weight > 500.0) {
		aqi = 500 + (int)(500.0 / 500.0 * (P25Weight - 500.0)); // Extension
	} else {
		aqi = -1; // Initializing
	}

	return aqi;
}


void DSM501::debug(void)
{
	Serial.println("--- DSM501 BEGIN ---");

	Serial.println(_tm_ms_start[0]);
	Serial.println(DSM501_MIN_RATIO_SPAN);
	Serial.println(DSM501_MIN_RATIO_SPAN - (millis() - _tm_ms_start[0]));

	Serial.println(getLowRatio(0));
	Serial.println(getLowRatio(1));

	Serial.println(_tm_us_total[0]);
	Serial.println(_tm_us_total[1]);
}
