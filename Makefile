SRCS=wlw.h main.c
CC=gcc
CFLAGS=-Wall -Wextra -Werror -static -Oz


default: main
	./main

wlw: wlw.h
	${CC} ${CFLAGS} -x c wlw.h -DWLW_EXAMPLE -o wlw

main: main.c
	${CC} ${CFLAGS} main.c -o main

$(SRCS): %: always
	@indent -gnu -nut -sar $(shell sed -n \
	               -e 's/^typedef.* \([^ ]*\);$$/-T\1/p' \
	               -e 's/^} \([^ ]*\);$$/-T\1/p' $@) $@

always: ;

