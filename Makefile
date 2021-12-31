build:
	mkdir -p bin
	gcc src/consumer.c -o bin/consumer.out -Wall -Wextra -O3
	gcc src/producer.c -o bin/producer.out -Wall -Wextra -O3

clean:
	rm -fr bin

all: clean build

produce:
	./bin/producer.out $(p) $(n)

consume:
	./bin/consumer.out $(c)
