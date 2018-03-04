"""Macros for defining static website content."""

_WEB_DIR = "web"

def erb_file(name, srcs, out, bootstrap=False, main_template='default'):
    """Renders a collection of eRuby templates to an output file.

    See the //webgen:erb tool for documentation on template syntax.

    Args:
      name: rule name.
      srcs: source templates to load and make available.
      out: output file path under website root.
      bootstrap: set to true to include bootstrap CSS and required scripts.
      main_template: entry point template name to render, 'default' by default.
    """

    erb_args = [
        "--out=$@",
        "--main=%s" % main_template,
    ]
    if bootstrap:
        erb_args.append("--style=bootstrap")
        erb_args.append("--script=jquery")
        erb_args.append("--script=popper")
        erb_args.append("--script=bootstrap")

    native.genrule(
        name = name,
        srcs = srcs,
        tools = ["@esowiki//webgen:erb"],
        outs = ["%s/%s" % (_WEB_DIR, out)],
        cmd = "ruby $(location @esowiki//webgen:erb) {args} {srcs}".format(
            args = " ".join(erb_args),
            srcs = " ".join(["$(locations %s)" % src for src in srcs])
        ),
    )

def js_file(name, srcs, out, level="SIMPLE"):
    """Compiles a set of JavaScript sources to a single minified script.

    The compilation is done using the Closure Compiler.

    Args:
      name: rule name.
      srcs: JavaScript source files, compiled in this order.
      out: output script path under website root.
      level: Closure Compiler optimization level, 'SIMPLE' by default.
    """

    native.genrule(
        name = name,
        srcs = srcs,
        tools = ["@esowiki//webgen:closure_compiler"],
        outs = ["%s/%s" % (_WEB_DIR, out), "%s/%s.map" % (_WEB_DIR, out)],
        cmd = """
            ./$(location @esowiki//webgen:closure_compiler) \
              --compilation_level {level} \
              --js_output_file $(location {out}) \
              --create_source_map $(location {out}.map) \
              {srcs}
        """.format(
            level = level,
            out = "%s/%s" % (_WEB_DIR, out),
            srcs = " ".join(["$(locations %s)" % src for src in srcs]),
        ),
    )

def scss_file(name, srcs, out, main=None):
    """Compiles a SCSS stylesheet into a minified .css file.

    Args:
      name: rule name.
      srcs: list of all .scss files needed by the main file.
      out: output .css file path under website root.
      main: main .scss file, by default the first element of srcs.
    """

    if not main: main = srcs[0]

    native.genrule(
        name = name,
        srcs = srcs,
        outs = ["%s/%s" % (_WEB_DIR, out), "%s/%s.map" % (_WEB_DIR, out)],
        cmd = "sassc -m -t compressed {main} {out}".format(
            main = "$(location %s)" % main,
            out = "$(location %s/%s)" % (_WEB_DIR, out),
        ),
    )

def web_data(name, src=None, out=None, srcs=[], map={}):
    """Copies static data files to include in a website.

    It's probably a good idea to provide just one out of the three
    optional source file arguments (src, srcs, map). You can provide
    multiple, and they will all be merged, but the behavior can be
    unintuitive.

    Each of the labels given as sources must map to a single output
    file. Otherwise figuring out the output path would be too tricky.
    When not specified (src without out, srcs), the output path will
    be the same as the input path.

    Args:
      name: rule name.
      src: single source file to include.
      out: output name of src under website root.
      srcs: list of source files to include.
      map: (input file -> output path) map.
    """

    map = dict(**map)
    if src:
        map[src] = out if out else src
    for s in srcs:
        map[s] = s

    native.genrule(
        name = name,
        srcs = map.keys(),
        outs = ["%s/%s" % (_WEB_DIR, o) for o in map.values()],
        cmd = ";".join([
            "cp $(location %s) $(location %s/%s)" % (s, _WEB_DIR, o)
            for s, o in map.items()
        ]),
    )

def website(name, srcs, preview_data=None, preview_redir={}):
    """Defines an entire website.

    This macro generates the following (external) targets:
    - name: archive ("name.zip") containing all website files.
    - name_preview: binary which will serve the website on a local port.

    The source labels must refer to instances of the other macros in
    this file (erb_file, js_file, scss_file, web_data). That's not
    strictly speaking enforced, but the process for figuring out file
    paths will break if that's not the case.

    The keys of the redirect map are regular expressions. They're
    matched against the incoming request path, anchored at the
    beginning. If a match is found, the preview server will use the
    corresponding map value as a URL to fetch, and return the result
    of that to the client. Any instances of $1, $2, ... in the target
    will be replaced by corresponding subgroups of the regular
    expression.

    Due to genrule trickery, you should avoid putting too special
    characters in the map. In particular, the characters ' ^ ` must
    not be used. Also remember to double \ characters in strings.

    Args:
      name: base name for generated targets.
      srcs: source files.
      preview_data: extra .zip file to serve in the preview server.
      preview_redir: paths the preview server will fetch from external server.
    """

    native.genrule(
        name = "%s" % name,
        srcs = srcs,
        outs = ["%s.zip" % name],
        tools = ["@esowiki//webgen:archive"],
        cmd = "./$(location @esowiki//webgen:archive) $@ " + " ".join(["$(locations %s)" % src for src in srcs]),
    )

    native.genrule(
        name = "%s_preview_stub" % name,
        srcs = [
            ":%s" % name,
            "@esowiki//webgen:preview_stub",
        ] + ([preview_data] if preview_data else []),
        outs = ["%s_preview.py" % name],
        cmd = """
            sed -e 's^%ARCHIVES%^{zip}{extra}^;s^%REDIR%^{redir}^' \
              < $(location @esowiki//webgen:preview_stub) \
              > $@
        """.format(
            zip = "$(location :%s)" % name,
            extra = " $(locations %s)" % preview_data if preview_data else "",
            redir = "`".join([k+"`"+v for k, v in preview_redir.items()]).replace("$","$$").replace("\\","\\\\"),
        ),
    )

    native.py_binary(
        name = "%s_preview" % name,
        srcs = [":%s_preview_stub" % name],
        deps = ["@esowiki//webgen:preview"],
        data = [":%s" % name] + ([preview_data] if preview_data else []),
    )
