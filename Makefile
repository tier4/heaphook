all: app preloaded.so

app: app.cpp
	g++ -g -o app app.cpp

preloaded.so: preloaded.cpp
	g++ -g -shared -fPIC -o preloaded.so preloaded.cpp

.PHONY: clean
clean:
	rm app preloaded.so core.*
