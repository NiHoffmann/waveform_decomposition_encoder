CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 -I.
LDLIBS  := -lm

BIN  := swe
SRCS := main.c sin_wave_encoder/sin_wave_encoder.c FFT/fft.c
OBJS := $(SRCS:.c=.o)

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all run clean
