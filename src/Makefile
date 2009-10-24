DB=postgres
CFLAGS=-Wall -Wextra -Wshadow -Werror -g -O0

STD_H=err.h types.h
BIN=brain train learn getreply hal

all: $(BIN)
clean:
	rm -f *.o $(BIN)

DB_OBJS=db.o db_conn_$(DB).o db_brain_$(DB).o db_word_$(DB).o db_list_$(DB).o db_map_$(DB).o db_model_$(DB).o

BRAIN=brain.o dict.o model.o console.o $(DB_OBJS)
brain: $(BRAIN)
	$(CC) -o $@ $(BRAIN) -lpq

TRAIN=train.o megahal.o console.o $(DB_OBJS)
train: $(TRAIN)
	$(CC) -o $@ $(TRAIN) -lpq

LEARN=learn.o megahal.o console.o $(DB_OBJS)
learn: $(LEARN)
	$(CC) -o $@ $(LEARN) -lpq

GETREPLY=getreply.o megahal.o console.o $(DB_OBJS)
getreply: $(GETREPLY)
	$(CC) -o $@ $(GETREPLY) -lpq

HAL=hal.o megahal.o console.o $(DB_OBJS)
hal: $(HAL)
	$(CC) -o $@ $(HAL) -lpq

db.h: types.h err.h

brain.o: dict.h model.h output.h db.h
train.o: megahal.h output.h db.h
learn.o: megahal.h output.h db.h
getreply.o: megahal.h output.h db.h
hal.o: megahal.h output.h db.h
megahal.o: dict.h model.h db.h

console.o: output.h
dict.o: db.h $(STD_H)
db.o: db.h $(STD_H)
db_conn_postgres.o: db.h db_postgres.h $(STD_H)
db_brain_postgres.o: db.h db_postgres.h $(STD_H)
db_word_postgres.o: db.h db_postgres.h $(STD_H)
db_list_postgres.o: db.h db_postgres.h $(STD_H)
db_map_postgres.o: db.h db_postgres.h $(STD_H)
db_model_postgres.o: db.h db_postgres.h $(STD_H)
model.o: db.h $(STD_H)
