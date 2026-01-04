CFLAGS=-Wall -Wextra -Werror

default: wlw

wlw: wlw.h
	cc wlw.h -x c -DWLW_EXAMPLE -o wlw

main: main.c
	cc main.c -o main
