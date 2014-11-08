CC=g++
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g -std=c++11
SRV=server
SEL_SRV=selectserver
CLT=client

all: $(SRV) $(CLT)

$(SRV):$(SRV).cpp
	$(CC) $(CCFLAGS) -o $(SRV) $(LIBSOCKET) $(SRV).cpp

$(CLT):	$(CLT).cpp
	$(CC) $(CCFLAGS) -o $(CLT) $(LIBSOCKET) $(CLT).cpp

clean:
	rm -f *.o *~
	rm -f $(SRV) $(CLT)
