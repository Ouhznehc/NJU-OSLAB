NAME := $(shell basename $(PWD))
export MODULE := M2
all: $(NAME)-64 $(NAME)-32
LDFLAGS += -lpthread

include ../Makefile

ifeq ($(DEBUG), 1)
	CFLAGS += -D__DEBUG__MODE_
endif

test:
	@rm -f $(NAME)-64 $(NAME)-32 $(NAME)-64.so $(NAME)-32.so
	@gcc -m32 $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)
ifeq ($(FILE), 1) 
	@./test $(TH) < input.txt 
else
	@./test $(TH) 
endif


