TARGETS = pattern

.PHONY: all clean

all: $(TARGETS)

clean:
	-rm $(TARGETS)
