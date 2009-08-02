DB=postgres
CFLAGS=-Wall -Werror -g -O0

STD_H=err.h types.h
BIN=brain

all: $(BIN)
clean:
	rm -f *.o $(BIN)

BRAIN=brain.o dict.o model.o console.o db.o db_conn_$(DB).o db_brain_$(DB).o db_word_$(DB).o db_list_$(DB).o db_map_$(DB).o
brain: $(BRAIN)
	$(CC) -o $@ $(BRAIN) -lpq

db.h: types.h

brain.o: dict.h output.h
cli.o: output.h
dict.o: db.h $(STD_H)
db.o: db.h $(STD_H)
db_conn_postgres.o: db.h db_postgres.h $(STD_H)
db_brain_postgres.o: db.h db_postgres.h $(STD_H)
db_word_postgres.o: db.h db_postgres.h $(STD_H)
db_list_postgres.o: db.h db_postgres.h $(STD_H)
db_map_postgres.o: db.h db_postgres.h $(STD_H)
model.o: db.h dbtree.h $(STD_H)
