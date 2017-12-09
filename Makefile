objs= coro_sched.o coro_switch.o memcache.o rbtree.o main.o

CC = gcc

CFLAGS = -g -pipe -Wall -O2
#CFLAGS = -g -pipe -Wall -O2 -std=gnu99

TARGET = manhattan

all:: $(TARGET)

${TARGET}:$(objs)
	${CC} -o $@ ${CFLAGS} $^ $(LIB)

%.o: %.cpp
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	@rm -rf $(objs) $(TARGET)
