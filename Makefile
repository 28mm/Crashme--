CCFLAGS=$(CFLAGS) $(LDFLAGS) $(CPPFLAGS) -std=gnu99

all: crashme crashme-kernel

crashme: crashme.c
	$(CC) $(CCFLAGS) -o crashme crashme.c

crashme-kernel: crashme-kernel.c
	$(CC) $(CCFLAGS) -o crashme-kernel crashme-kernel.c

clean:
	-rm crashme
	-rm crashme-kernel
