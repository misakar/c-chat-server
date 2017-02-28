CFLAGS = -Ideps/uv/include -Ideps/uv/src -Wall
LDFLAGS = -lm

.PHONY: chat-server clean # not associate with a file

chat-server: src/main.o deps/uv/libuv.a
	$(CC) $^ -o $@ $(LDFLAGS)

deps/uv/libuv.a:
	$(MAKE) all -C deps/uv

clean:
	rm -rf chat-server src/main.o
