CC=gcc-13
CFLAGS=-std=gnu11 -g -O3 -pthread -Wall -Werror -pedantic
# ASAN = Address SANitizer
CFLAGS_ASAN=-std=gnu11 -g -Og -pthread -fsanitize=address -Wall -Werror -pedantic
# TSAN = Thread SANitizer
CFLAGS_TSAN=-std=gnu11 -g -Og -pthread -fsanitize=thread -fsanitize=undefined -Wall -Werror -pedantic

# Main compile scripts:
clean:
	rm -f *.o parallel life life-asan life-tsan

serial:
	gcc ./main.c ./life.c ./life-serial.c -o serial

parallel:
	gcc ./cyclic_barrier.c ./split_board.c ./life.c ./life-parallel.c ./workers.c ./main.c -o parallel

mem:
	gcc -Wall -Wextra -Werror -pedantic -g ./cyclic_barrier.c ./split_board.c ./life.c ./life-parallel.c ./workers.c ./main.c -o parallel_mem
	valgrind --leak-check=full --show-leak-kinds=all ./parallel_mem 8 ./input/4505x1008 1> /dev/null 2> valgrind_report.txt
	rm parallel_mem

all: life life-asan life-tsan



%-asan.o: %.c
	$(CC) -c $(CFLAGS_ASAN) -o $@ $<

%-tsan.o: %.c
	$(CC) -c $(CFLAGS_TSAN) -o $@ $<

life.o: life.c life.h
life-asan.o: life.c life.h
life-tsan.o: life.c life.h

life-parallel.o: life-parallel.c life.h
life-parallel-asan.o: life-parallel.c life.h
life-parallel-tsan.o: life-parallel.c life.h

life-serial.o: life-serial.c life.h
life-serial-asan.o: life-serial.c life.h
life-serial-tsan.o: life-serial.c life.h

main.o: main.c life.h
main-asan.o: main.c life.h
main-tsan.o: main.c life.h

life: main.o life.o life-parallel.o life-serial.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

life-asan: main-asan.o life-asan.o life-parallel-asan.o life-serial-asan.o
	$(CC) $(CFLAGS_ASAN) -o $@ $^ -lpthread

life-tsan: main-tsan.o life-tsan.o life-parallel-tsan.o life-serial-tsan.o
	$(CC) $(CFLAGS_TSAN) -o $@ $^ -lpthread