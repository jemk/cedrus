TARGET = mpeg-test
SRC = main.c mpeg.c ../common/disp.c ../common/ve.c
CFLAGS = -Wall -Wextra -O3
CFLAGS += $(shell pkg-config --cflags libavformat libavcodec)
LDFLAGS = $(shell pkg-config --libs libavformat libavcodec)
