TARGET = h264enc
SRC = main.c ../common/ve.c
CFLAGS = -Wall -O3 -I../common
LDFLAGS = -lpthread
LIBS =
CC = gcc

MAKEFLAGS += -rR --no-print-directory

DEP_CFLAGS = -MD -MP -MQ $@

OBJ = $(addsuffix .o,$(basename $(SRC)))
DEP = $(addsuffix .d,$(basename $(SRC)))

.PHONY: clean all

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

clean:
	rm -f $(OBJ)
	rm -f $(DEP)
	rm -f $(TARGET)

%.o: %.c
	$(CC) $(DEP_CFLAGS) $(CFLAGS) -c $< -o $@

include $(wildcard $(DEP))
