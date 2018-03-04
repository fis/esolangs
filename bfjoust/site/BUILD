load("//webgen:webgen.bzl", "erb_file", "js_file", "scss_file", "web_data", "website")
load(":site.bzl", "page", "script")

# main tabs and their associated custom scripts

page(name="index_html", file="index.html")
script(name="submit_js", file="js/submit.js")

page(name="scores_html", file="scores.html", data=True)
script(name="scores_js", file="js/scores.js")

page(name="matrix_html", file="matrix.html", data=True)
script(name="matrix_js", file="js/matrix.js")

page(name="breakdown_html", file="breakdown.html", data=True)
script(name="breakdown_js", file="js/breakdown.js")

page(name="game_html", file="game.html", data=True)
script(
    name="egojsout_js",
    files=["js/egojsout/egojsout.js", "js/egojsout/ejsoutTrace.js", "js/egojsout/ejsoutAnim.js"],
    out="js/egojsout.js",
)

page(name="vis_html", file="vis.html")

page(name="internals_html", file="internals.html", math=True)

# visualization plots and their specific scripts

page(name="vis_cycles_html", file="vis/cycles.html", out="vis.cycles.html", plot=True)
script(name="vis_cycles_js", file="vis/js/cycles.js", out="js/vis.cycles.js", plot=True)

page(name="vis_wins_html", file="vis/wins.html", out="vis.wins.html", plot="prog")
script(name="vis_wins_js", file="vis/js/wins.js", out="js/vis.wins.js", plot="prog")

page(name="vis_tape_heat_html", file="vis/tape_heat.html", out="vis.tape_heat.html", plot="tape")
script(name="vis_tape_heat_js", file="vis/js/tape_heat.js", out="js/vis.tape_heat.js", plot="tape")

page(name="vis_tape_abs_html", file="vis/tape_abs.html", out="vis.tape_abs.html", plot="tape")
script(name="vis_tape_abs_js", file="vis/js/tape_abs.js", out="js/vis.tape_abs.js", plot="tape")

# shared scripts

script(name="zhill_js", file="js/zhill.js")

# stylesheets

scss_file(name = "main_css", srcs = ["scss/main.scss", "scss/_open-iconic.scss"], out = "css/main.css")
scss_file(name = "vis_css", srcs = ["scss/vis.scss"], out = "css/vis.css")
scss_file(name = "vis_tape_css", srcs = ["scss/vis_tape.scss", "scss/vis.scss"], out = "css/vis_tape.css")

# full site contents

web_data(
    name = "fonts",
    srcs = ["fonts/open-iconic." + ext for ext in ["eot", "otf", "svg", "ttf", "woff"]],
)

website(
    name = "site",
    srcs = [
        ":breakdown_html",
        ":breakdown_js",
        ":egojsout_js",
        ":fonts",
        ":game_html",
        ":index_html",
        ":internals_html",
        ":main_css",
        ":matrix_html",
        ":matrix_js",
        ":scores_html",
        ":scores_js",
        ":submit_js",
        ":vis_css",
        ":vis_cycles_html",
        ":vis_cycles_js",
        ":vis_tape_abs_html",
        ":vis_tape_abs_js",
        ":vis_tape_css",
        ":vis_tape_heat_html",
        ":vis_tape_heat_js",
        ":vis_wins_html",
        ":vis_wins_js",
        ":vis_html",
        ":zhill_js",
    ],
    preview_redir = {
        "/report\\.js": "http://zem.fi/bfjoust/report.js",
        "/data/(.*)": "http://zem.fi/bfjoust/vis/data/$1",
        "/repo/plain/(.*)\\.bfjoust\\?id=(.*)": "http://zem.fi/git/?p=hill;a=blob_plain;f=$1.bfjoust;hb=$2",
    },
)
