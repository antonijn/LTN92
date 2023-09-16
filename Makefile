HEADERS = my_font_letters.h my_font_numbers.h my_font_symbols.h

all: $(HEADERS)

%.h: %.png
	./compile.py $^ > $@
