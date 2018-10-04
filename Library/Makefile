CC=gcc
CFLAGS=-I. -g
DEPS = function_calls.h 
OBJ = fiber_app.o function_calls.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

fiber_app: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
default: fiber_app

clean: 
	rm -rfv fiber_app *.o
