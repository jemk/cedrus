TARGET = mpeg-test
SRC = main.c mpeg.c ../common/disp.c ../common/ve.c
CFLAGS = -Wall -Wextra -O3 -I../common
CFLAGS += $(shell pkg-config --cflags libavformat libavcodec)
LDFLAGS = -lpthread $(shell pkg-config --libs libavformat libavcodec)
