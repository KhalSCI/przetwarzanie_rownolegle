CC      = gcc
CFLAGS  = -O3 -fopenmp -Wall
DIRS    = PI1 PI2 PI3 PI4 PI5 PI6 PI7

all: $(DIRS:%=%/pi)

PI1/pi: PI1/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI2/pi: PI2/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI3/pi: PI3/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI4/pi: PI4/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI5/pi: PI5/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI6/pi: PI6/pi.c ; $(CC) $(CFLAGS) $< -o $@
PI7/pi: PI7/pi.c ; $(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(DIRS:%=%/pi)

.PHONY: all clean
