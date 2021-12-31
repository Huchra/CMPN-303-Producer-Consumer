build:
	mkdir -p bin
	gcc consumer.c -o bin/consumer.out -Wall -Wextra
	gcc producer.c -o bin/producer.out -Wall -Wextra

clean:
	rm -fr bin

all: clean build
#TODO: FIX this, KOSOM ZIOYAD
run:
	./bin/producer.out $(rate)
	./bin/consumer.out $(rate)

#ok ha cancel kosom el make
#poc?
#u said poc? ui said momken badal el make pwc 
#ah ok
#yeah ok i'm just looking @ how to run targets in parallel
#ok i'LL PULL I HATE U