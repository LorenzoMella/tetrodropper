CFLAGS = -g -O0 -D NDEBUG
LDFLAGS = -lncurses

all: tetrodropper

tetrodropper: tetrodropper.c

clean:
	rm -f tetrodropper
