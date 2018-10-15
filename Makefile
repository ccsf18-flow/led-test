LD_PATH=-L musl/lib -L rpi_ws281x/
experiment: experiment.o rpi_ws281x/libws2811.a musl/lib/libc.a
	arm-linux-gnueabihf-gcc -o $@ \
	  experiment.o rpi_ws281x/libws2811.a -lrt --static

experiment.o : experiment.c
	arm-linux-gnueabihf-gcc --static -c -o $@ $<
