# Must be possibly different from default port in battle.c
# As not necessary, uses same default PORT number
PORT=58487 

# The flags for the compiler
CFLAGS= -DPORT=$(PORT) -g -Wall -Werror

# Makes an executable named battle
TARGET=battle

# Default behaviour / what the make file will run
all: $(TARGET)

$(TARGET): battle.c
	$(CC) $(CFLAGS) -o $(TARGET) battle.c

# Removes the executable made if called as "make clean"
clean:
	rm -f $(TARGET)
