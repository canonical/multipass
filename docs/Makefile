# Minimal makefile for Sphinx documentation
#
# Add your customisation to `Makefile` instead.

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXDIR       = .sphinx
SPHINXOPTS      ?= -c . -d $(SPHINXDIR)/.doctrees -j auto
SPHINXBUILD     ?= $(VENVDIR)/bin/sphinx-build
SOURCEDIR       = .
BUILDDIR        = _build
VENVDIR         = $(SPHINXDIR)/venv
PA11Y           = $(SPHINXDIR)/node_modules/pa11y/bin/pa11y.js --config $(SPHINXDIR)/pa11y.json
VENV         	   = $(VENVDIR)/bin/activate
TARGET          = *
ALLFILES        =  *.rst **/*.rst
METRICSDIR      = $(SOURCEDIR)/.sphinx/metrics
REQPDFPACKS     = latexmk fonts-freefont-otf texlive-latex-recommended texlive-latex-extra texlive-fonts-recommended texlive-font-utils texlive-lang-cjk texlive-xetex plantuml xindy tex-gyre dvipng
CONFIRM_SUDO    ?= N

# Put it first so that "make" without argument is like "make help".
help:
	@echo "\n" \
        "------------------------------------------------------------- \n" \
        "* watch, build and serve the documentation:  make run \n" \
        "* only build:                                make html \n" \
        "* only serve:                                make serve \n" \
        "* clean built doc files:                     make clean-doc \n" \
        "* clean full environment:                    make clean \n" \
        "* check links:                               make linkcheck \n" \
        "* check spelling:                            make spelling \n" \
        "* check spelling (without building again):   make spellcheck \n" \
        "* check inclusive language:                  make woke \n" \
        "* check accessibility:                       make pa11y \n" \
        "* check style guide compliance:              make vale \n" \
        "* check style guide compliance on target:    make vale TARGET=* \n" \
        "* check metrics for documentation:           make allmetrics \n" \
        "* other possible targets:                    make <TAB twice> \n" \
        "------------------------------------------------------------- \n"

.PHONY: full-help woke-install spellcheck-install pa11y-install install run html \
        epub serve clean clean-doc spelling spellcheck linkcheck woke \
        allmetrics pa11y pdf-prep-force pdf-prep pdf Makefile.sp vale bash

full-help: $(VENVDIR)
	@. $(VENV); $(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	@echo "\n\033[1;31mNOTE: This help texts shows unsupported targets!\033[0m"
	@echo "Run 'make help' to see supported targets."

# If requirements are updated, venv should be rebuilt and timestamped.
$(VENVDIR):
	python3 -c "import venv" || \
        (echo "You must install python3-venv before you can build the documentation."; exit 1)
	@echo "... setting up virtualenv"
	python3 -m venv $(VENVDIR)
	. $(VENV); pip install $(PIPOPTS) --require-virtualenv \
	    --upgrade -r $(SPHINXDIR)/requirements.txt \
            --log $(VENVDIR)/pip_install.log
	@test ! -f $(VENVDIR)/pip_list.txt || \
            mv $(VENVDIR)/pip_list.txt $(VENVDIR)/pip_list.txt.bak
	@. $(VENV); pip list --local --format=freeze > $(VENVDIR)/pip_list.txt
	@touch $(VENVDIR)

woke-install:
	@type woke >/dev/null 2>&1 || \
	{ \
		echo "Installing system-wide \"woke\" snap..."; \
		confirm_sudo=$(CONFIRM_SUDO); \
		if [ "$$confirm_sudo" != "y" ] && [ "$$confirm_sudo" != "Y" ]; then \
			read -p "This requires sudo privileges. Proceed? [y/N]: " confirm_sudo; \
		fi; \
		if [ "$$confirm_sudo" = "y" ] || [ "$$confirm_sudo" = "Y" ]; then \
			sudo snap install woke; \
		else \
			echo "Installation cancelled."; \
		fi \
	}	

spellcheck-install:
	@type aspell >/dev/null 2>&1 || \
	{ \
		echo "Installing system-wide \"aspell\" packages..."; \
		confirm_sudo=$(CONFIRM_SUDO); \
		if [ "$$confirm_sudo" != "y" ] && [ "$$confirm_sudo" != "Y" ]; then \
			read -p "This requires sudo privileges. Proceed? [y/N]: " confirm_sudo; \
		fi; \
		if [ "$$confirm_sudo" = "y" ] || [ "$$confirm_sudo" = "Y" ]; then \
			sudo apt-get install aspell aspell-en; \
		else \
			echo "Installation cancelled."; \
		fi \
	}

pa11y-install:
	@type $(PA11Y) >/dev/null 2>&1 || { \
			echo "Installing \"pa11y\" from npm... \n"; \
			mkdir -p $(SPHINXDIR)/node_modules/ ; \
			npm install --prefix $(SPHINXDIR) pa11y; \
		}

install: $(VENVDIR)

run: install
	. $(VENV); $(VENVDIR)/bin/sphinx-autobuild -b dirhtml "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS)

# Doesn't depend on $(BUILDDIR) to rebuild properly at every run.
html: install
	. $(VENV); $(SPHINXBUILD) -W --keep-going -b dirhtml "$(SOURCEDIR)" "$(BUILDDIR)" -w $(SPHINXDIR)/warnings.txt $(SPHINXOPTS)

epub: install
	. $(VENV); $(SPHINXBUILD) -b epub "$(SOURCEDIR)" "$(BUILDDIR)" -w $(SPHINXDIR)/warnings.txt $(SPHINXOPTS)

serve: html
	cd "$(BUILDDIR)"; python3 -m http.server --bind 127.0.0.1 8000

clean: clean-doc
	@test ! -e "$(VENVDIR)" -o -d "$(VENVDIR)" -a "$(abspath $(VENVDIR))" != "$(VENVDIR)"
	rm -rf $(VENVDIR)
	rm -rf $(SPHINXDIR)/node_modules/
	rm -rf $(SPHINXDIR)/styles
	rm -rf $(SPHINXDIR)/vale.ini

clean-doc:
	git clean -fx "$(BUILDDIR)"
	rm -rf $(SPHINXDIR)/.doctrees

spellcheck: spellcheck-install
	. $(VENV) ; python3 -m pyspelling -c $(SPHINXDIR)/spellingcheck.yaml -j $(shell nproc)

spelling: html spellcheck

linkcheck: install
	. $(VENV) ; $(SPHINXBUILD) -b linkcheck "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) || { grep --color -F "[broken]" "$(BUILDDIR)/output.txt"; exit 1; }
	exit 0

woke: woke-install
	woke $(ALLFILES) --exit-1-on-failure \
	    -c https://raw.githubusercontent.com/canonical/Inclusive-naming/main/config.yml

pa11y: pa11y-install html
	find $(BUILDDIR) -name *.html -print0 | xargs -n 1 -0 $(PA11Y)

vale: install
	@. $(VENV); test -d $(SPHINXDIR)/venv/lib/python*/site-packages/vale || pip install vale
	@. $(VENV); test -f $(SPHINXDIR)/vale.ini || python3 $(SPHINXDIR)/get_vale_conf.py
	@. $(VENV); find $(SPHINXDIR)/venv/lib/python*/site-packages/vale/vale_bin -size 195c -exec vale --config "$(SPHINXDIR)/vale.ini" $(TARGET) > /dev/null \;
	@cat $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept.txt > $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept_backup.txt
	@cat $(SPHINXDIR)/.wordlist.txt $(SOURCEDIR)/.custom_wordlist.txt >> $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept.txt
	@echo ""
	@echo "Running Vale against $(TARGET). To change target set TARGET= with make command"
	@echo ""
	@. $(VENV); vale --config "$(SPHINXDIR)/vale.ini" --glob='*.{md,txt,rst}' $(TARGET) || true
	@cat $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept_backup.txt > $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept.txt && rm $(SPHINXDIR)/styles/config/vocabularies/Canonical/accept_backup.txt

pdf-prep: install
	@for packageName in $(REQPDFPACKS); do (dpkg-query -W -f='$${Status}' $$packageName 2>/dev/null | \
        grep -c "ok installed" >/dev/null && echo "Package $$packageName is installed") && continue || \
        (echo "\nPDF generation requires the installation of the following packages: $(REQPDFPACKS)" && \
        echo "" && echo "Run 'sudo make pdf-prep-force' to install these packages" && echo "" && echo \
        "Please be aware these packages will be installed to your system") && exit 1 ; done

pdf-prep-force:
	apt-get update
	apt-get upgrade -y
	apt-get install --no-install-recommends -y $(REQPDFPACKS) \

pdf: pdf-prep
	@. $(VENV); sphinx-build -M latexpdf "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS)
	@rm ./$(BUILDDIR)/latex/front-page-light.pdf || true
	@rm ./$(BUILDDIR)/latex/normal-page-footer.pdf || true
	@find ./$(BUILDDIR)/latex -name "*.pdf" -exec mv -t ./$(BUILDDIR) {} +
	@rm -r $(BUILDDIR)/latex
	@echo "\nOutput can be found in ./$(BUILDDIR)\n"

allmetrics: html
	@echo "Recording documentation metrics..."
	@echo "Checking for existence of vale..."
	. $(VENV)
	@. $(VENV); test -d $(SPHINXDIR)/venv/lib/python*/site-packages/vale || pip install vale
	@. $(VENV); test -f $(SPHINXDIR)/vale.ini || python3 $(SPHINXDIR)/get_vale_conf.py
	@. $(VENV); find $(SPHINXDIR)/venv/lib/python*/site-packages/vale/vale_bin -size 195c -exec vale --config "$(SPHINXDIR)/vale.ini" $(TARGET) > /dev/null \;
	@eval '$(METRICSDIR)/source_metrics.sh $(PWD)'
	@eval '$(METRICSDIR)/build_metrics.sh $(PWD) $(METRICSDIR)'

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%:
	. $(VENV); $(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
