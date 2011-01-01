BINS= \
	pdb_gen \
	find_syscall \
	pup

all: $(BINS)

clean:
	rm -f $(BINS) *~
