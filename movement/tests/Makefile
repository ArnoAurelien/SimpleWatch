TOP = ../..
include $(TOP)/make.mk
INCLUDES += -I../ -I../watch_faces/clock/

TEST_SRCS = travel_clock_test.c \
  ../watch_faces/clock/travel_clock.c \
  ../watch_faces/clock/france_clock_face.c \
  ../watch_faces/clock/finland_clock_face.c \
  ../watch_faces/clock/full_date_face.c \
  $(TOP)/watch-library/shared/watch/watch_utility.c

.PHONY: test
test:
	$(CC) $(INCLUDES) $(DEFINES) -Wall -Wextra -Wno-unused-parameter -sSINGLE_FILE=1 $(TEST_SRCS) -o /tmp/sensor-watch-clock-test.js
	node /tmp/sensor-watch-clock-test.js
