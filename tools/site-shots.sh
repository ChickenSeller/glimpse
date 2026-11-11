#!/bin/sh
# Renders the screenshots of the homepage and the guide (docs/public/images)
# in every UI language with glimpse-site-shots (tests/SiteShots.cpp).
#
#   tools/site-shots.sh <build dir> [shot...]
#
# Chinese goes to the root, English and Japanese to en/ and ja/, as the
# guide's pages. Recognition uses PaddleOCR and translation Google's free
# service: the models must be downloaded and the network reachable.
set -e
build=${1:?usage: tools/site-shots.sh <build dir> [shot...]}
shift
images=$(cd "$(dirname "$0")/../docs/public/images" && pwd)
for pair in zh_CN: en:en/ ja:ja/; do
    language=${pair%%:*}
    "$build/glimpse-site-shots" "$images/${pair#*:}" "$language" "$@"
done
