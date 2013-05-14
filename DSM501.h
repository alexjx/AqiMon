#ifndef DSM501_H
#define DSM501_H
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

typedef unsigned long ulong_t;

#define _uS_By_S(x)	((x) * 1000ul * 1000ul)
#define _mS_By_S(x)	((x) * 1000ul)
#define _S_By_S(x) 	(x)

#define DSM501_MIN_SIG_SPAN		(15ul)			// 10uS
#define DSM501_MIN_RATIO_SPAN	_mS_By_S(600ul)	// 10mins

#define MAX_LOW_RATIO_OF_1400ug	(15.63)

#define PM10_PIN	A3
#define PM25_PIN	A2

enum State {
	S_Idle,
	S_Start,
};

class DSM501 {
public:
	static DSM501* getInst(void) {
		return &_instance;
	}
	void begin();
	void update(); // called in the loop function for update

	double 	getLowRatio(int i = 0);
	ulong_t getParticalPcs(int i = 0);
	double  getParticalWeight(int i = 0);
	double  getParticalWeightHigh(int i = 0);
	double  getParticalWeightLow(int i = 0);
	int 	getAQI();

	void 	debug();

	static void Isr();

protected:
	DSM501();
	void signal_begin(int i);
	void signal_end(int i);

protected:
	static DSM501 _instance;

private:
	const static int _pin[2];
	ulong_t _tm_us_total[2];  	// total low in microsec
	ulong_t	_tm_ms_start[2];	// start time in millisec
	ulong_t _tm_us_sig_start[2];  // period start time in millisec
	double 	_lastLowRatio[2];

	volatile State _state[2];
	volatile int _pinState[2];
};

#endif
