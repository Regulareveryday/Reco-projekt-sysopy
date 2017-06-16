all:
	make compile

compile:
	gcc -o server server.c -pthread
	gcc -o reco reco.c

clean:
	rm reco server
