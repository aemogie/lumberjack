SRCS=wlw.c main.c
CC=gcc
CFLAGS=-ansi -pedantic -Wall -Wextra -Werror -Wno-unused-function -static -Oz


default: main
	./main

main: main.c wlw.c
	$(CC) $(CFLAGS) main.c -o main

$(SRCS): %: always
	@indent -gnu -nut -sar $(shell sed -n \
	               -e 's/^typedef.* (\*\([^)]*\).*;$$/-T\1/p' \
	               -e 's/^typedef.* \([^ ]*\);$$/-T\1/p' \
	               -e 's/^} \([^ ]*\);$$/-T\1/p' $@) $@

always: ;

