# Configuration file for the Sphinx documentation builder.
#
# For a full list of options, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys

sys.path.insert(0, os.path.abspath("../.."))

from triumvirate import __version__ as version


# -- Project information -------------------------------------------------

project = 'Triumvirate'
copyright = '2023'
author = 'Mike S Wang & Naonori S Sugiyama'
release = version


# -- General configuration -----------------------------------------------

exclude_patterns = ['setup', 'config', 'tests', 'examples']

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.coverage',
    'sphinx.ext.githubpages',
    'sphinx.ext.intersphinx',
    'sphinx.ext.mathjax',
    'sphinx.ext.napoleon',
    'sphinx.ext.todo',
    'sphinx.ext.viewcode',
    'myst_nb',
]

master_doc = 'index'

pygments_style = 'sphinx'

source_suffix = ['.rst', '.txt', '.md']

templates_path = ['_templates']


# -- Options for HTML output ---------------------------------------------

html_favicon = '_static/Triumvirate.ico'

html_logo = '_static/Triumvirate.png'

html_static_path = ['_static']

html_theme = 'sphinx_book_theme'

html_theme_options = {
    'repository_url' : 'https://github.com/MikeSWang/Triumvirate',
    'home_page_in_toc': True,
    'logo_only': True,
    'use_download_button': False,
    'use_fullscreen_button': False,
    'use_repository_button': True,
}


# -- Extension configuration ---------------------------------------------

autodoc_member_order = 'bysource'

intersphinx_mapping = {
    'python': ("https://docs.python.org/3", None),
    'pytest': ("https://docs.pytest.org/en/latest/", None),
    'numpy': ("https://numpy.org/doc/stable/", None),
    'scipy': ("https://docs.scipy.org/doc/scipy/reference", None),
    'matplotlib': ("https://matplotlib.org", None),
}

napoleon_include_special_with_doc = True
napoleon_google_docstring = False
napoleon_use_param = False
napoleon_use_ivar = True

nb_execution_mode = 'off'

todo_include_todos = True
