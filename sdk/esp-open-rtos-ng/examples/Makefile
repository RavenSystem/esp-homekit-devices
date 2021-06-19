EXAMPLES = $(shell find $(dir $(lastword $(MAKEFILE_LIST))) -mindepth 2 -name Makefile | sed s/Makefile//g)
# Generate some dummy .dummybuild/.dummyrebuild target files
EXAMPLES_BUILD = $(patsubst %,%.dummybuild,$(EXAMPLES))
EXAMPLES_REBUILD = $(patsubst %,%.dummyrebuild,$(EXAMPLES))
EXAMPLES_CLEAN = $(patsubst %,%.dummyclean,$(EXAMPLES))

warning:
	@echo "******************************************************"
	@echo "You may not want this Makefile, even though it's here!"
	@echo "******************************************************"
	@echo ""
	@echo "SUGGESTIONS:"
	@echo "Running 'make' in one of the subdirectories of examples/ will build a single example."
	@echo "Running 'make help' in one of the subdirectories of examples/ will print some help."
	@echo ""
	@echo "OTHERWISE:"
	@echo "This makefile is for building all of the examples at once, as a developer test."
	@echo "To use it, run 'make build-examples' or 'make rebuild-examples'"
	@echo

build-examples: $(EXAMPLES_BUILD)

rebuild-examples: $(EXAMPLES_REBUILD)

clean-examples: $(EXAMPLES_CLEAN)

%.dummybuild:
	$(MAKE) -C $(dir $@)

%.dummyrebuild:
	$(MAKE) -C $(dir $@) rebuild

%.dummyclean:
	$(MAKE) -C $(dir $@) clean

.PHONY: warning rebuild-examples build-examples clean-examples
.NOTPARALLEL:
.ONESHELL:
