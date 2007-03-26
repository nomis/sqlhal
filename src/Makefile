DB=postgres
CFLAGS=-Wall -g -O0

STD_H=err.h types.h
BIN=../bin/brain

all: $(BIN)
clean:
	rm -f *.o $(BIN)

BRAIN=brain.o dict.o db_$(DB).o
../bin/brain: $(BRAIN)
	$(CC) -o $@ $(BRAIN) -lpq

db.h: types.h

brain.c: dict.h
dict.c: db.h $(STD_H)
db_postgres.c: db.h $(STD_H)
