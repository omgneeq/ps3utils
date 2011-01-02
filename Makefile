CFLAGS=-Wall -Wextra \
        -Wundef \
        -Wnested-externs \
        -Wwrite-strings \
        -Wpointer-arith \
        -Wbad-function-cast \
        -Wmissing-declarations \
        -Wmissing-prototypes \
        -Wstrict-prototypes \
        -Wredundant-decls \
        -Wno-unused-parameter \
        -Wno-missing-field-initializers


BINS= \
	pdb_gen \
	find_syscall \
	pup

all: $(BINS)

clean:
	rm -f $(BINS) *~
