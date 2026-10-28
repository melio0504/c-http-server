# Setup all the variables we need
#
# I also learn that Makefile has four different kinds of assignment
# We have ":=", "=", "?=", and "+="
# So what I am using right now is ":=" which is evaluated only once
# at the initial occurance.
CC 		:= gcc
CFLAGS 	:= -Wall -Wextra -Werror
SRC_DIR := src
BIN_DIR := bin
TARGET 	:= $(BIN_DIR)/server
SOURCES := $(SRC_DIR)/server.c

# And here we have the PHONY! ohh my little PHONY~ lol
# So anyway, this is a special directive used to declare
# targets that don't represent existing files.
.PHONY: all setup clean

all: $(TARGET)

setup:
		mkdir -p $(BIN_DIR)

$(TARGET): $(SOURCES) | setup 
		$(CC) $(CFLAGS) -o $@ $^

clean: 
		rm -f $(TARGET)
