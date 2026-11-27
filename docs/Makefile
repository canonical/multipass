# Minimal makefile for Sphinx documentation
#
# Add your customisation to `Makefile` instead.

# You can set these variables from the command line, and also
# from the environment for the first two.

DEV_DIR          ?= _dev
SPHINX_OPTS      ?= -c . -d $(DEV_DIR)/.doctrees -j auto
SPHINX_BUILD     ?= $(DOCS_VENVDIR)/bin/sphinx-build
SPHINX_HOST      ?= 127.0.0.1
SPHINX_PORT      ?= 8000
SPHINX_AUTOBUILD_OPTS ?= -D llms_txt_enabled=0
DOCS_VENVDIR     ?= .venv
DOCS_VENV        ?= $(DOCS_VENVDIR)/bin/activate
DOCS_SOURCEDIR   ?= .
DOCS_BUILDDIR    ?= _build
DOCS_PDFPACKAGES ?= latexmk fonts-freefont-otf texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended texlive-font-utils texlive-lang-cjk texlive-xetex plantuml xindy tex-gyre dvipng
DOCS_VOCAB       ?= $(DEV_DIR)/styles/config/vocabularies/Canonical
VALE_DIR         ?= $(DOCS_VENVDIR)/lib/python*/site-packages/vale
VALE_CONFIG      ?= $(DEV_DIR)/vale.ini
PA11Y_CMD        ?= $(DEV_DIR)/node_modules/pa11y/bin/pa11y.js --config $(DEV_DIR)/pa11y.json
CONFIRM_SUDO     ?= N
CHECK_PATH       ?= $(filter-out $(DOCS_VENVDIR) $(DOCS_BUILDDIR) $(DEV_DIR),$(wildcard *))

# Put it first so that "make" without argument is like "make help".
help:
	@echo
	@echo "-------------------------------------------------------------"
	@echo "* watch, build and serve the documentation:  make run"
	@echo "* only build:                                make html"
	@echo "* only serve:                                make serve"
	@echo "* clean built doc files:                     make clean-doc"
	@echo "* clean full environment:                    make clean"
	@echo "* check links:                               make linkcheck"
	@echo "* check markdown:                            make lint-md"
	@echo "* check spelling:                            make spelling"
	@echo "* check spelling (without building again):   make spellcheck"
	@echo "* check inclusive language:                  make woke"
	@echo "* check accessibility:                       make pa11y"
	@echo "* check style guide compliance:              make vale"
	@echo "* check style guide compliance on target:    make vale CHECK_PATH=*"
	@echo "* other possible targets:                    make <TAB twice>"
	@echo "-------------------------------------------------------------"
	@echo

.PHONY: help full-help html epub pdf linkcheck spelling spellcheck woke \
        vale pa11y run serve install pa11y-install   \
        vale-install pdf-prep pdf-prep-force clean clean-doc \
        update lint-md

full-help: $(DOCS_VENVDIR)
	@. $(DOCS_VENV); $(SPHINX_BUILD) -M help "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" $(SPHINX_OPTS) $(O)
	@echo "\n\033[1;31mNOTE: This help texts shows unsupported targets!\033[0m"
	@echo "Run 'make help' to see supported targets."

# If requirements are updated, venv should be rebuilt and timestamped.
$(DOCS_VENVDIR):
	@echo "... setting up virtualenv"
	python3 -m venv $(DOCS_VENVDIR) || { echo "You must install python3-venv before you can build the documentation."; exit 1; }
	. $(DOCS_VENV); pip install $(PIPOPTS) --require-virtualenv \
	    --upgrade -r requirements.txt \
            --log $(DOCS_VENVDIR)/pip_install.log
	@test ! -f $(DOCS_VENVDIR)/pip_list.txt || \
            mv $(DOCS_VENVDIR)/pip_list.txt $(DOCS_VENVDIR)/pip_list.txt.bak
	@. $(DOCS_VENV); pip list --local --format=freeze > $(DOCS_VENVDIR)/pip_list.txt
	@touch $(DOCS_VENVDIR)

pa11y-install:
	@test -x $(firstword $(PA11Y_CMD)) >/dev/null || { \
			echo "Installing \"pa11y\" from npm..."; echo; \
			mkdir -p $(DEV_DIR)/node_modules/ ; \
			npm install --prefix $(DEV_DIR) pa11y; \
		}

pymarkdownlnt-install: install
	@. $(DOCS_VENV); test -d $(DOCS_VENVDIR)/lib/python*/site-packages/pymarkdown || pip install pymarkdownlnt==0.9.35

install: $(DOCS_VENVDIR)

run: install
	. $(DOCS_VENV); $(DOCS_VENVDIR)/bin/sphinx-autobuild -b dirhtml --host $(SPHINX_HOST) --port $(SPHINX_PORT) "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" $(SPHINX_OPTS) $(SPHINX_AUTOBUILD_OPTS)

# Does not depend on $(DOCS_BUILDDIR) to rebuild properly at every run.
html: install
	. $(DOCS_VENV); $(SPHINX_BUILD) --fail-on-warning --keep-going -b dirhtml "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" -w $(DEV_DIR)/warnings.txt $(SPHINX_OPTS)

epub: install
	. $(DOCS_VENV); $(SPHINX_BUILD) -b epub "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" -w $(DEV_DIR)/warnings.txt $(SPHINX_OPTS)

serve: html
	cd "$(DOCS_BUILDDIR)"; python3 -m http.server --bind $(SPHINX_HOST) $(SPHINX_PORT)

clean: clean-doc
	@test ! -e "$(DOCS_VENVDIR)" -o -d "$(DOCS_VENVDIR)" -a "$(abspath $(DOCS_VENVDIR))" != "$(DOCS_VENVDIR)"
	rm -rf $(DOCS_VENVDIR)
	rm -rf $(DEV_DIR)/node_modules/
	rm -rf $(DEV_DIR)/styles
	rm -rf $(VALE_CONFIG)

clean-doc:
	git clean -fx "$(DOCS_BUILDDIR)"
	rm -rf $(DEV_DIR)/.doctrees

linkcheck: install
	. $(DOCS_VENV) ; $(SPHINX_BUILD) -b linkcheck -q "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" $(SPHINX_OPTS) || { grep --color -F "[broken]" "$(DOCS_BUILDDIR)/output.txt"; exit 1; }
	exit 0

pa11y: pa11y-install html
	find $(DOCS_BUILDDIR) -name *.html -print0 | xargs -n 1 -0 $(PA11Y_CMD)

# Without --return-code-scheme explicit, pymarkdownlnt returns 1 for multiple scenarios
# By using the explicit scheme, it only returns 1 when no files are found,
# which should not result in failure
lint-md: pymarkdownlnt-install
	@. $(DOCS_VENV); pymarkdownlnt \
		--config $(DEV_DIR)/.pymarkdown.json \
		--return-code-scheme explicit \
		scan --recurse $(CHECK_PATH); \
	status=$$?; \
	if [ $$status -eq 1 ]; then \
		echo "No Markdown files selected for linting"; \
		exit 0; \
	fi; \
	echo "pymarkdownlnt exited with code $$status"; \
	exit $$status;

vale-install: install
	@. $(DOCS_VENV); test -f $(VALE_CONFIG) || python3 $(DEV_DIR)/get_vale_conf.py
	@echo '.Name=="Canonical.400-Enforce-inclusive-terms"' > $(DEV_DIR)/styles/woke.filter
	@echo '.Level=="error" and .Name!="Canonical.500-Repeated-words" and .Name!="Canonical.000-US-spellcheck"' > $(DEV_DIR)/styles/error.filter
	@echo '.Name=="Canonical.000-US-spellcheck"' > $(DEV_DIR)/styles/spelling.filter
	@. $(DOCS_VENV); find $(VALE_DIR)/vale_bin -size 195c -exec vale --version \;

woke: vale-install
	@cat $(DOCS_VOCAB)/accept.txt > $(DOCS_VOCAB)/accept_backup.txt
	@cat $(DOCS_SOURCEDIR)/.custom_wordlist.txt >> $(DOCS_VOCAB)/accept.txt
	@echo "Running Vale acceptable term check against $(CHECK_PATH). To change target set CHECK_PATH= with make command"
	@. $(DOCS_VENV); vale --config="$(VALE_CONFIG)" --filter='$(DEV_DIR)/styles/woke.filter' --glob='*.{md,rst}' $(CHECK_PATH)
	@cat $(DOCS_VOCAB)/accept_backup.txt > $(DOCS_VOCAB)/accept.txt && rm $(DOCS_VOCAB)/accept_backup.txt

vale: vale-install
	@cat $(DOCS_VOCAB)/accept.txt > $(DOCS_VOCAB)/accept_backup.txt
	@cat $(DOCS_SOURCEDIR)/.custom_wordlist.txt >> $(DOCS_VOCAB)/accept.txt
	@echo "Running Vale against $(CHECK_PATH). To change target set CHECK_PATH= with make command"
	@. $(DOCS_VENV); vale --config="$(VALE_CONFIG)" --filter='$(DEV_DIR)/styles/error.filter' --glob='*.{md,rst}' $(CHECK_PATH)
	@cat $(DOCS_VOCAB)/accept_backup.txt > $(DOCS_VOCAB)/accept.txt && rm $(DOCS_VOCAB)/accept_backup.txt

spelling: vale-install
	@cat $(DOCS_VOCAB)/accept.txt > $(DOCS_VOCAB)/accept_backup.txt
	@cat $(DOCS_SOURCEDIR)/.custom_wordlist.txt >> $(DOCS_VOCAB)/accept.txt
	@echo "Running Vale against $(CHECK_PATH). To change target set CHECK_PATH= with make command"
	@. $(DOCS_VENV); vale --config="$(VALE_CONFIG)" --filter='$(DEV_DIR)/styles/spelling.filter' --glob='*.{md,rst}' $(CHECK_PATH)
	@cat $(DOCS_VOCAB)/accept_backup.txt > $(DOCS_VOCAB)/accept.txt && rm $(DOCS_VOCAB)/accept_backup.txt

spellcheck: spelling
	@echo "Please note that the \`make spellcheck\` command is being deprecated in favor of \`make spelling\`"

pdf-prep: install
	@for packageName in $(DOCS_PDFPACKAGES); do (dpkg-query -W -f='$${Status}' $$packageName 2>/dev/null | \
        grep -c "ok installed" >/dev/null && echo "Package $$packageName is installed") && continue || \
        (echo; echo "PDF generation requires the installation of the following packages: $(DOCS_PDFPACKAGES)" && \
        echo "" && echo "Run 'sudo make pdf-prep-force' to install these packages" && echo "" && echo \
        "Please be aware these packages will be installed to your system") && exit 1 ; done

pdf-prep-force:
	apt-get update
	apt-get install --no-install-recommends -y $(DOCS_PDFPACKAGES) \

pdf: pdf-prep
	@. $(DOCS_VENV); $(SPHINX_BUILD) -M latexpdf "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" $(SPHINX_OPTS)
	@rm ./$(DOCS_BUILDDIR)/latex/front-page-light.pdf || true
	@rm ./$(DOCS_BUILDDIR)/latex/normal-page-footer.pdf || true
	@find ./$(DOCS_BUILDDIR)/latex -name "*.pdf" -exec mv -t ./$(DOCS_BUILDDIR) {} +
	@rm -r $(DOCS_BUILDDIR)/latex
	@echo
	@echo "Output can be found in ./$(DOCS_BUILDDIR)"
	@echo

update: install
	@. $(DOCS_VENV); _dev/update_sp.py

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINX_OPTS).
%:
	$(MAKE) --no-print-directory install
	. $(DOCS_VENV); $(SPHINX_BUILD) -M $@ "$(DOCS_SOURCEDIR)" "$(DOCS_BUILDDIR)" $(SPHINX_OPTS) $(O)
