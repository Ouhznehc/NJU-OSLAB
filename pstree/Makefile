NAME := $(shell basename $(PWD))
export MODULE := M1
all: $(NAME)-64 $(NAME)-32
include ../Makefile

test: commit-and-make
	@echo "-----pstree-32-----" 
	@./pstree-32 $(ARGS)
	@echo "\n"
# @echo "-----pstree-native----"
# @$(ARGS)
# @echo "\n"
# @echo "-----pstree-64-----" 
# @./pstree-64 $(ARGS)
# @echo "\n"

