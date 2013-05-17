#ifndef DSM501_H
#define DSM501_H
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include "EEPROM.h"


#define EA_DSM501_COEFF	0

typedef unsigned long ulong_t;

#define _mS_By_S(x)	((x) * 1000ul)
#define _S_By_S(x) 	(x)

#define DSM501_MIN_SIG_SPAN	(10ul)			// 10mS
#define DSM501_MAX_SIG_SPAN	(90ul)			// 90mS
#define DSM501_MIN_WIN_SPAN	_mS_By_S(60ul)	// 60S

#define PM10_PIN	A3
#define PM25_PIN	A2

#define PM10_IDX	0
#define PM25_IDX	1

#define SAF_WIN_MAX 30	// mins

enum State {
	S_Idle,
	S_Start,
};

class DSM501 {
public:
	DSM501(int pin10 = A3, int pin25 = A2);
	void begin();
	void update(); // called in the loop function for update

	double 	getLowRatio(int i = 0);
	double  getParticalWeight(int i = 0);
	int 	getAQI();

	void 	debug();

	uint8_t getCoeff() const {
		return _coeff;
	}

	uint8_t setCoeff(uint8_t coeff) {
		_coeff = coeff;
		EEPROM.write(EA_DSM501_COEFF, _coeff);
		return _coeff;
	}

protected:
	void signal_begin(int i);
	void signal_end(int i);

private:
	int 	_pin[2];
	State   _state[2];
	ulong_t _low_total[2];
	ulong_t	_win_start[2];
	ulong_t _sig_start[2];

	// Sliding Averaging Filtering
	ulong_t _saf_sum[2];
	ulong_t _saf_ent[2][SAF_WIN_MAX];
	ulong_t	_saf_idx[2];

	uint8_t	_coeff;
	double 	_lastLowRatio[2];
};

#endif
