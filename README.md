# timer
time wheel timer.

## example
```
 #include "time_wheel.h"
 void main() {
    // create timer.
    STimeWheelSpace::CTimerRegister timer;

    // add once timer.
	timer.add_once_timer([](void*) {
		printf("0 timer out");
	}, 0, 10 * 1000);

    // add repeated timer
	timer.add_repeated_timer([](void*) {
		printf("1 timer out");
		timer.kill_timer(1);
	}, 1, 1000);

    // run the timer.
	for (;;) {
		STimeWheelSpace::CTimeWheel::instance().run();
	}
 }
```
