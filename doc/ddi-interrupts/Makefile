SVGS = ihdl-state.svg psm-intrs.svg dev-tree-parent.svg dev-tree-intr-parent.svg
PAGES = interrupts.html

all: $(SVGS) $(PAGES)

clean:
	$(RM) $(SVGS) $(PAGES)

.KEEP_STATE:

.SUFFIXES: .gv .svg .txt .adoc .html

.adoc.html:
	asciidoctor -v $< -o $@

.gv.svg:
	dot -Tsvg $< -o $@

.gv.txt:
	graph-easy $< > $@
