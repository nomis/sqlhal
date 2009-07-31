DB=postgres
CFLAGS=-Wall -Werror -g -O0

STD_H=err.h types.h
BIN=brain

all: $(BIN)
clean:
	rm -f *.o $(BIN)

BRAIN=brain.o dict.o model.o console.o db.o db_$(DB).o
brain: $(BRAIN)
	$(CC) -o $@ $(BRAIN) -lpq

db.h: types.h

brain.o: dict.h output.h
cli.o: output.h
dict.o: db.h $(STD_H)
db_postgres.o: db.h $(STD_H)
model.o: db.h dbtree.h $(STD_H)
