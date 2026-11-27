import datetime
import os
import textwrap

# Configuration for the Sphinx documentation builder.
# All configuration specific to your project should be done in this file.
#
# If you're new to Sphinx and don't want any advanced or custom features,
# just go through the items marked 'TODO'.
#
# A complete list of built-in Sphinx configuration values:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
#
# The Sphinx Stack uses the Ulwazi theme:
# https://github.com/canonical/ulwazi

#######################
# Project information #
#######################

# Project name
project = "Multipass"

# Author name; used in the default copyright statement in the page footer
author = "Canonical Ltd."

# The year in the copyright statement
copyright = f"{datetime.date.today().year}"

# String appended to the title of individual HTML pages
html_title = project + " documentation"

# Documentation website URL
#
# The docs are published at https://canonical.com/multipass/docs/, not at the
# Read the Docs URL, so the canonical URL is built from the RTD version slug.
version_slug = f"{os.environ.get('READTHEDOCS_VERSION', 'local')}"
html_baseurl = f"https://canonical.com/multipass/docs/{version_slug}/"
ogp_site_url = html_baseurl

# Preview name of the documentation website
ogp_site_name = project

# Preview image URL
ogp_image = "https://assets.ubuntu.com/v1/253da317-image-document-ubuntudocs.svg"

# Product favicon; shown in bookmarks, browser tabs, etc.
# TODO: To customise the favicon, uncomment and update the next line.
# html_favicon = "_static/favicon.png"

# The theme for the HTML output
html_theme = "ulwazi"

# Dictionary of values to pass into every page's template.
html_context = {
    # Product page URL; can be different from product docs URL
    "product_page": "canonical.com/multipass",
    # Community links for the 'Resources' dropdown menu.
    "discourse": "https://discourse.ubuntu.com/c/project/multipass/21",
    "mattermost": "",
    "matrix": "https://app.element.io/#/room/#Multipass:matrix.org",
    "github_url": "https://github.com/canonical/multipass",
    # Docs location in the repo; used in links for viewing the source files
    "repo_folder": "/docs/",
    # Source file extension used in edit and view links
    "default_source_extension": ".md",
    # Feedback and edit/view links require a valid `github_url`.
    "feedback": True,
    # Brand configuration for the leftmost entry in the banner
    "logo_title": "Canonical",
    "logo_link_URL": "https://canonical.com",
    "logo_img_URL": "https://assets.ubuntu.com/v1/82818827-CoF_white.svg",
    # Google Tag Manager container ID
    "tag_id": "GTM-KNX3CJC",
    # License information to be rendered in the footer
    "author": author,
    "license": {
        "name": "CC-BY-SA",
        "url": "",
    },
}

#######################
# Sitemap configuration: https://sphinx-sitemap.readthedocs.io/
#######################

# sphinx-sitemap uses html_baseurl to generate the full URL for each page:
sitemap_url_scheme = "{link}"

# Required to avoid sitemap conflicts with canonical.com
sitemap_filename = "doc-sitemap.xml"

# Include `lastmod` dates in the sitemap:
sitemap_show_lastmod = True

# Pages excluded from the sitemap:
sitemap_excludes = [
    "404/",
    "genindex/",
    "search/",
]

################################
# Template and asset locations #
################################

html_static_path = ["_static"]

#############
# Redirects #
#############

# Add redirects to the 'redirects.txt' file
# https://sphinxext-rediraffe.readthedocs.io/en/latest/

# To set up redirects in the Read the Docs project dashboard:
# https://docs.readthedocs.io/en/stable/guides/redirects.html

rediraffe_redirects = "redirects.txt"

# Strips '/index.html' from destination URLs when building with 'dirhtml'
rediraffe_dir_only = True


############################
# sphinx-llm configuration #
############################

# This description is included in llms.txt to provide some initial context for your
# product docs.
llms_txt_description = textwrap.dedent(
    """\
    This is the documentation for Multipass, a tool to generate cloud-style virtual
    machines quickly on Linux, macOS and Windows.
    """
)

# Generate an 'llms-full.txt' file containing all of the documentation content
llms_txt_full_build = True

# Suppress warnings from upstream markdown builder
llms_txt_suppress_unknown_node_warnings = True

# The base URL for references built by sphinx-markdown-builder.
if os.environ.get("READTHEDOCS"):
    markdown_http_base = html_baseurl

###########################
# Link checker exceptions #
###########################

# A regex list of URLs that are ignored by 'make linkcheck'
linkcheck_ignore = [
    r"https://app\.element\.io/",
    "https://localhost:8081",
    "https://github.com/canonical/*",
    "https://sourceforge.net/projects/vcxsrv/",
    "https://sourceforge.net/projects/xming/",
    "http://www.straightrunning.com/XmingNotes/",
    "https://unix.stackexchange.com",  # it seems stackexchange is now blocking bots
    "https://developer.hashicorp.com/packer",
    "https://www.freedesktop.org/*",
    "https://asciinema.org/*",
    "https://askubuntu.com/a/4404",
]

# A regex list of URLs where anchors are ignored by 'make linkcheck'
linkcheck_anchors_ignore_for_url = [r"https://github\.com/", r"https://matrix\.to/"]

# Give linkcheck multiple tries on failure
linkcheck_retries = 3

# Excluded documents; past release notes are not maintained, but the index is
linkcheck_exclude_documents = [r"reference/release-notes/(?!index$)"]

########################
# Configuration extras #
########################

# Custom Sphinx extensions; see
# https://www.sphinx-doc.org/en/master/usage/extensions/index.html
extensions = [
    "ulwazi",
    "myst_parser",
    "sphinx_design",
    "sphinx_rerediraffe",
    "sphinxcontrib.jquery",
    "sphinxext.opengraph",
    "sphinx_config_options",
    "sphinx_llm.txt",
    "sphinx_modern_pdf_style",
    "sphinx_related_links",
    "sphinx_roles",
    "sphinx_terminal",
    "sphinx_youtube_links",
    "sphinxcontrib.cairosvgconverter",
    "sphinx.ext.intersphinx",
    "sphinx_sitemap",
    # Project-specific extensions
    "sphinx_structured_toc",
]

# Excludes files or directories from processing
exclude_patterns = [
    "doc-cheat-sheet*",
    ".venv*",
    "_dev",
    "reference/release-notes/RELEASE_NOTES_TEMPLATE.md",
]

# Adds custom CSS files, located remotely or in 'html_static_path'.
html_css_files = [
    "https://assets.ubuntu.com/v1/d86746ef-cookie_banner.css",
]

# Adds custom JavaScript files, located remotely or in 'html_static_path'.
html_js_files = [
    "https://assets.ubuntu.com/v1/287a5e8f-bundle.js",
    # Rewrites links to the Read the Docs domain so they point to canonical.com
    "js/overwrite_links.js",
]

# Project slug
slug = "multipass/docs"

# Apply modern PDF styling and configuration
set_modern_pdf_config = True

# Your manpage URL
# TODO: To enable manpage links, uncomment and replace {codename} with required
#       release, preferably an LTS release (e.g. noble). Do *not* substitute
#       {section} or {page}; these will be replaced by sphinx at build time
#
# If set, the :manpage: role creates an inline link to the manual page.
# manpages_url = 'https://manpages.ubuntu.com/manpages/{codename}/en/' + \
#     'man{section}/{page}.{section}.html'

# Specifies a reST snippet to be prepended to each .rst file
# This defines a :center: role that centers table cell content.
rst_prolog = """
.. role:: center
   :class: align-center
.. role:: h2
    :class: hclass2
.. role:: woke-ignore
    :class: woke-ignore
.. role:: vale-ignore
    :class: vale-ignore
"""

# Configuration for Intersphinx projects
#
# intersphinx_mapping = {
#     "snap": ("https://snapcraft.io/docs/", None),
# }
