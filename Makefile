
hexcreator: $(wildcard *.c)
	gcc -Wall -Wextra -Werror $< -o $@

clean:
	rm -rf hexcreator

test: hexcreator force
	rm -rf test
	mkdir test
	head -c 1M /dev/urandom > test/sample.bin
	./hexcreator test/sample.bin test/ 4-4-16 @D@ 8 2048 "set cuts(%,%,%) @" 0 > test/cuts.tcl
	xxd -e -g 8 -c 8 test/sample.bin | awk '{printf("%s\n", $$2);}' > test/reference.hex
	head test/main.hex
	head test/reference.hex 
	diff -q test/reference.hex test/main.hex

.phony: test

force:
	@: