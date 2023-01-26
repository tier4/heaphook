all: app preloaded.so heaplog_parser

app: app.cpp
	g++ -g -o app app.cpp

preloaded.so: preloaded.cpp
	g++ -g -shared -fPIC -o preloaded.so preloaded.cpp

heaplog_parser: heaplog_parser.cpp
	g++ -o heaplog_parser heaplog_parser.cpp

.PHONY: run
run:
	LD_PRELOAD=./preloaded.so ./app

.PHONY: clean
clean:
	rm app preloaded.so heaplog_parser core.* *.log *.pdf

