LDFLAGS += -pthread
CXXFLAGS = -g
LINK.o = $(LINK.cpp)
uthread: uthread.o main.o
uthread.o: uthread.h
main.o: uthread.h
clean:
	$(RM) -f uthread uthread.o main.o
