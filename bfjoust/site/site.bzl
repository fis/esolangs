load("//webgen:webgen.bzl", "erb_file", "js_file", "scss_file", "website")

def page(name, file, out=None, data=False, math=False, plot=False):
    extra_templates = []
    if data: extra_templates.append("template/data.html")
    if math: extra_templates.append("template/mathjax.html")
    if plot: extra_templates.append("template/plot.html")
    if plot == "tape": extra_templates.append("template/plot_tape.html")

    erb_file(
        name = name,
        srcs = [file, "template/default.html"] + extra_templates,
        out = out if out else file,
        bootstrap = True,
    )

def script(name, file=None, files=[], out=None, plot=False):
    extra_sources = []
    if plot: extra_sources.append("vis/js/plot.js")
    if plot == "prog" or plot == "tape": extra_sources.append("vis/js/plot_prog.js")
    if plot == "tape": extra_sources.append("vis/js/plot_tape.js")

    js_file(
        name = name,
        srcs = extra_sources + ([file] if file else []) + files,
        out = out if out else file,
    )
