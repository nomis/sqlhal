DB=postgres
CFLAGS=-Wall -Werror -g -O0

STD_H=err.h types.h
BIN=../bin/brain

all: $(BIN)
clean:
	rm -f *.o $(BIN)

BRAIN=brain.o dict.o console.o db_$(DB).o
../bin/brain: $(BRAIN)
	$(CC) -o $@ $(BRAIN) -lpq

db.h: types.h

brain.o: dict.h output.h
cli.o: output.h
dict.o: db.h $(STD_H)
db_postgres.o: db.h $(STD_H)
