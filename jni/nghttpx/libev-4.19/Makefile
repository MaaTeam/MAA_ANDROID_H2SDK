TARGET = libev.a

DEPS = \
	ev.o \
	event.o

$(TARGET):$(DEPS)
	$(AR) rcs $(TARGET) $(DEPS)

clean:
	rm -f $(TARGET) $(DEPS)