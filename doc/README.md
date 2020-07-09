# Flux Core Documentation

Flux-core documentation currently consists of man pages written in ReStructured Text (rst). The man pages are generated using sphinx and are also hosted at flux-framework.readthedocs.io.

##  Build Instructions

To build with python virtual environments:

```bash
virtualenv -p python3 sphinx-rtd
source sphinx-rtd/bin/activate
git clone git@github.com:flux-framework/flux-core
cd flux-core/doc
pip install -r requirements.txt
make man
```

## Adding a New Man Page

There are 2 steps to adding a man page:
- creating the man page documentation file
- configuring the generation of the man page(s)

### Creating the Man Page

Man pages are written as [ReStructured Text](https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html) (`.rst`) files.
We use [Sphinx](https://www.sphinx-doc.org/en/master/) to process the documentation files and turn them into man pages (troff) and web pages (html).

Sphinx will automatically adds the following sections to the generated man page (so do not include them in the `.rst` file):

- `NAME` (first section)
- `AUTHOR` (penultimate section)
- `COPYRIGHT` (final section)

Each section title should be underlined with `=`

### Configuring Generation

Generating a man pages is done via the `man_pages` variable in `conf.py`. For example:

```
man_pages = [
    ('man1/flux', 'flux', 'the Flux resource management framework', [author], 1),
]
```

The tuple entry in the `man_pages` list specifies:
- File name (relative path, without the `.rst` extension)
- Name of man page
- Description of man page
- Author (use `[author]` as in the example)
- Manual section for the generated man page

It is possible for multiple man pages to be generated from a single source file.
Simple create an entry for each man page you want generated.
These entries can have the same file path, but different man page names.
