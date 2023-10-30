ifeq ($(COSMO),)
COSMO := /opt/cosmo/
endif

COSMOCC=$(COSMO)/bin/cosmocc

.PHONY: build
build: xasm.com

%.com: %.c
	$(COSMOCC) -o $@ $^

clean:
	rm -f xasm.com*