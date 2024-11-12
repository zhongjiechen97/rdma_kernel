cc:=gcc
src:=$(wildcard *.c)
user_app:=rdma_user
cflags:=-g
libs:=-libverbs -lrdmacm

all: $(user_app)

$(user_app): $(patsubst %.c,%.o,$(src))
	$(cc) $^ $(libs) -o $@

$(patsubst %.c,%.o,$(src)): %.o: %.c
	$(cc) -c $(cflags) $^ -o $@

.PHONY: clean
clean:
ifneq ($(shell ls $(user_app) $(patsubst %.c,%.o,$(src)) 2> /dev/null),)
	rm $(foreach i, $(shell ls $(user_app) $(patsubst %.c,%.o,$(src)) 2> /dev/null),$(i))
endif

