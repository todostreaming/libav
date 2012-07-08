OUTDIR=../msvc-out

SOURCES=$(shell find .. -name \*.[c,h] -a ! -path ../msvc\* )

TARGETS=$(subst ..,$(OUTDIR),$(SOURCES))

$(TARGETS): c99-to-msvc
	@mkdir -p $(dir $@)
	@echo GEN $@
	@./c99-to-msvc $(subst $(OUTDIR),.., $@) 2>/dev/null | \
		sed -e "s/\r\n/\n/" -e "s:\\* *\(restrict[ )]\):*__\1:g" \
		> $@

convert: $(TARGETS)

c99-to-msvc: c99-to-msvc.c
	$(CC) $^ -o $@

clean:
	@rm -fR $(OUTDIR)

.PHONY: convert clean
