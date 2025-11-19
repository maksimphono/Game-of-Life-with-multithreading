-include conf/lab.mk
-include conf/info.mk

CC=gcc-13
CFLAGS=-std=gnu11 -g -O3 -pthread -Wall -Werror -pedantic
# ASAN = Address SANitizer
CFLAGS_ASAN=-std=gnu11 -g -Og -pthread -fsanitize=address -Wall -Werror -pedantic
# TSAN = Thread SANitizer
CFLAGS_TSAN=-std=gnu11 -g -Og -pthread -fsanitize=thread -fsanitize=undefined -Wall -Werror -pedantic

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

clean:
	rm -f *.o life life-asan life-tsan

STYLE=\033[1;31m
NC=\033[0m

info-check:
	@if test -z "$(SID)"; then \
		echo "${STYLE}Please set SID in conf/info.mk${NC}"; \
		false; \
	fi
	@if test -z "`echo $(SID) | grep '^[0-9]\{9\}$$'`"; then \
		echo -n "${STYLE}Your SID (${SID}) does not appear to be correct. Continue? [y/N]${NC} "; \
		read -p "" r; \
		test "$$r" = y; \
	fi
	@if test -z "$(TOKEN)"; then \
		echo "${STYLE}Please set TOKEN in conf/info.mk${NC}"; \
		false; \
	fi

submit: clean info-check
	curl -F "token=${TOKEN}" -F "lab_num=${LAB_NUM}" -F "file=@life-parallel.c" http://114.212.81.7:9999/upload_code

report: info-check
	@if ! test -f $(SID).pdf; then \
		echo "${STYLE}Please put your report in a file named $(SID).pdf${NC}"; \
		false; \
	fi
	curl -F "token=${TOKEN}" -F "lab_num=${LAB_NUM}" -F "file=@${SID}.pdf" http://114.212.81.7:9999/upload_report

score: info-check
	curl "http://114.212.81.7:9999/download?token=${TOKEN}&lab_num=${LAB_NUM}"

.PHONY: all clean info-check submit report score
