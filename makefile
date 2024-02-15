CC=clang++

all:
	@mkdir -p build
	$(CC) main.cpp -o ./build/qbe-test

run: all
	./build/qbe-test ./test.lang
	qbe -o ./build/output.s ./build/output.ssa
	as -o ./build/output.o ./build/output.s
	$(CC) -o ./build/main ./build/output.o
	@rm -rf ./build/output.o ./build/output.ssa ./build/output.s
