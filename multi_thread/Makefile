SRC = 	src/calqueue.c \
		src/channel.c \
		src/jmp.S \
		src/main.c \
		src/model.c \
		src/numerical.c \
		src/thread.c \
		src/function_calls.c\
		src/ult.c
		
CFLAGS = -O3
EXEC = test
OBJS := $(SRC:%.c=%.o)
OBJS := $(OBJS:%.S=%.o)
LIBS = -lm -lpthread

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $(EXEC)

%.o : %.S
	$(CC) $< $(LIBS) -c -o $@

.PHONY: clean
clean:
	-rm $(OBJS)
	-rm $(EXEC)
