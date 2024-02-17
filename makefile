CC=clang++

all:
	@mkdir -p build
	$(CC) main.cpp -o ./build/qbe-test

run: all
	./build/qbe-test ./test.lang
	qbe -o ./build/output.s ./build/output.ssa
	$(CC) -o ./build/main ./build/output.s
	@rm -rf ./build/output.ssa ./build/output.s
