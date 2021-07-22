TARGET = final_project

ALT_DEVICE_FAMILY ?= soc_cv_av

HWLIBS_ROOT = ../opencv/hwlib

CFLAGS = -ggdb -Wall -std=c++11 `pkg-config --cflags opencv` -I$(HWLIBS_ROOT)/include -I$(HWLIBS_ROOT)/include/$(ALT_DEVICE_FAMILY) -D$(ALT_DEVICE_FAMILY)
LDFLAGS = -ggdb -Wall -std=c++11 `pkg-config --libs opencv` -lpthread
//CROSS_COMPILE = arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)g++

all: $(TARGET)

$(TARGET): $(TARGET).o D8MCapture.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.a *.o *~

