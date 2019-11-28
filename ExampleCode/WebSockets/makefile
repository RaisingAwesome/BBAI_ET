# Learn about make files here:  https://www3.ntu.edu.sg/home/ehchua/programming/cpp/gcc_make.html
all: w

w: wally.o
	g++ -o w wally.o -lssl -lcrypto

wally.o: wally.cpp
	sudo g++ -c wally.cpp

clean: 
	rm w wally.o
