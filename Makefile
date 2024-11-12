CC=gcc
SOURCE=cp2p.c
OBJ=$(SOURCE:.c=.o)
EXE=cp2p

all: $(EXE) clean execute

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $@

.o: .c
	$(CC) -c $< -o $@

clean:
	rm -rf $(OBJ)

execute:
	./$(EXE) 44444