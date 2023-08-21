#!/bin/sh

pushd ../src/wifi-gateway-ui

rm -rf dist/
parcel build --no-source-maps --no-optimize main.html
cd dist
bundled_js_filename=$(ls *.js)
bundled_js_contents=$(cat "$bundled_js_filename")

export script_tag_to_replace='<script type="module" src="/'$bundled_js_filename'"></script>'
export script_tag_to_insert='<script type="module">'$bundled_js_contents'</script>'

<"main.html" awk '
    s=index($0,ENVIRON["script_tag_to_replace"]){$0=substr($0, 1, s-1) ENVIRON["script_tag_to_insert"] substr($0, s + length(ENVIRON["script_tag_to_replace"]))} 1
' > "wifi-gateway-ui.html"

popd

./generate-embedded-html.sh ../src/wifi-gateway-ui/dist/wifi-gateway-ui.html
