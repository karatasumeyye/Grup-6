#Grup 6 
# Dilara Karaoğlan 1/B B231210374
# Melih Yasak 1/A B221210079
# Sümeyye Karataş 1/B B221210005
# Damla Korkmaz 1/B B221210098
# Mahdi Shahrouei 1/A B221210559

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
