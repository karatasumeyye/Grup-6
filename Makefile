# Compiler and flags
CC = gcc
CFLAGS = 

# Output executable name
TARGET = main

# Source file
SRCS = main.c

# Default rule to build the executable
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SRCS)
	$(CC) -o $(TARGET) $(SRCS)

# Rule to clean up build artifacts
clean:
	rm -f $(TARGET)

# Rule to run the program
run: $(TARGET)
	./$(TARGET)
