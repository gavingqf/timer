# timer
time wheel timer.

## example
```
 #include <stdlib.h>
 #include <chrono>
 #include "time_wheel.h"
 // define sleepMS.
 #define sleepMS(x) std::this_thread::sleep_for(std::chrono::microseconds(x))
 void main() {
    // create timer.
    STimeWheelSpace::CTimerRegister timer;

    // add 0 once timer(10s).
    timer.add_once_timer([](void*) {
	printf("0 timer out");
    }, 0, 10 * 1000);

    // add 1 repeated timer(1s).
    timer.add_repeated_timer([&timer](void*) {
	printf("1 timer out");

        // kill the 1 timer.
	timer.kill_timer(1);
    }, 1, 1000);

    // run the timer.
    for (;;) {
	STimeWheelSpace::CTimeWheel::instance().run();
	sleepMS(10);
    }
 }
```
