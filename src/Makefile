CFLAGS += -g -O0
override CFLAGS += -Wall -Wextra -Wshadow -Werror
LDLIBS += -lpq

DB=postgres
STD_H=err.h types.h
BIN=brain train learn getreply hal

all: $(BIN)
clean:
	rm -f *.o $(BIN)

DB_OBJS=db.o db_conn_$(DB).o db_brain_$(DB).o db_word_$(DB).o db_list_$(DB).o db_map_$(DB).o db_model_$(DB).o
MH_OBJS=megahal.o megahal_string.o dict.o

BRAIN=brain.o dict.o model.o console.o $(DB_OBJS)
brain: $(BRAIN)

TRAIN=train.o $(MH_OBJS) console.o $(DB_OBJS)
train: $(TRAIN)

LEARN=learn.o $(MH_OBJS) quiet.o $(DB_OBJS)
learn: $(LEARN)

GETREPLY=getreply.o $(MH_OBJS) quiet.o $(DB_OBJS)
getreply: $(GETREPLY)

HAL=hal.o $(MH_OBJS) quiet.o $(DB_OBJS)
hal: $(HAL)

db.h: types.h err.h

brain.o: dict.h model.h output.h db.h
train.o: megahal.h output.h db.h
learn.o: megahal.h output.h db.h
getreply.o: megahal.h output.h db.h
hal.o: megahal.h output.h db.h

console.o: output.h
dict.o: db.h dict.h $(STD_H)
db.o: db.h $(STD_H)
db_conn_postgres.o: db.h db_postgres.h $(STD_H)
db_brain_postgres.o: db.h db_postgres.h $(STD_H)
db_word_postgres.o: db.h db_postgres.h $(STD_H)
db_list_postgres.o: db.h db_postgres.h $(STD_H)
db_map_postgres.o: db.h db_postgres.h $(STD_H)
db_model_postgres.o: db.h db_postgres.h $(STD_H)
megahal.o: dict.h model.h db.h $(STD_H)
megahal_string.o: dict.h db.h $(STD_H)
model.o: db.h model.h $(STD_H)
quiet.o: output.h
