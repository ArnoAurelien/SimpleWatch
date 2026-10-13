TOP = ../..
include $(TOP)/make.mk
INCLUDES += -I../ -I../watch_faces/clock/

.PHONY: test
test:
	$(CC) $(INCLUDES) $(DEFINES) -Wall -Wextra -Wno-unused-parameter -sSINGLE_FILE=1 travel_clock_test.c $(TOP)/watch-library/shared/watch/watch_utility.c -o /tmp/sensor-watch-clock-test.js
	node /tmp/sensor-watch-clock-test.js
