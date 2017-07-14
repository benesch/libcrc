CRC32_DEPS := crc32.o crc32_amd64.o

libcrc32.a: $(CRC32_DEPS)
	ar $@ $^

crc32_test: $(CRC32_DEPS) crc32_test.o

.PHONY: clean
.clean:
	rm -rf crc32_test *.o
