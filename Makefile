CC = gcc
CFLAGS = -std=c89 -pedantic
OFLAGS = -lm
COMMON_DEPS = src/Headers/*.h
OBJ = build/Master.o build/Node.o build/User.o

all: bin/Master

build/%.o: src/C/%.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

bin/Master: $(OBJ) $(COMMON_DEPS)  
	$(CC) -o bin/Master build/Master.o build/Node.o build/User.o $(OFLAGS)

clean:
	$(RM) build/*
	$(RM) bin/*
