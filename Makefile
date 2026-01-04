CFLAGS=-Wall -Wextra -Werror

default: wlw
	./wlw

wlw: wlw.h
	cc ${CFLAGS} -x c wlw.h -DWLW_EXAMPLE -o wlw

wlw.h: always
	clang-format -i wlw.h

main: main.c
	cc ${CFLAGS} main.c -o main

main.c:
	clang-format -i main.c

always:

