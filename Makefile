build:
	mkdir -p bin
	gcc consumer.c -o bin/consumer.out -Wall -Wextra
	gcc producer.c -o bin/producer.out -Wall -Wextra

clean:
	rm -fr bin

all: clean build

run:
	./bin/producer.out $(rate)
	./bin/consumer.out $(rate)

