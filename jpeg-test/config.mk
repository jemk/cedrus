TARGET = jpeg-test
SRC = main.c jpeg.c ../common/disp.c ../common/ve.c
CFLAGS = -Wall -Wextra -O3 -I../common
LDFLAGS = -lpthread
