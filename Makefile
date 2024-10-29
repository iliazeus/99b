all: 99b 99b_printf 99b_slow time

clean:
	rm -f 99b 99b_printf 99b_slow

time: 99b 99b_printf 99b_slow
	for f in $^; do echo $$f; time ./$$f > /dev/null; done

%: %.cpp
	g++ -std=c++23 -O3 -flto $< -o $@
