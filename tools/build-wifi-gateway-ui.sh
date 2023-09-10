#!/bin/sh

export REPOSITORY_BASE_PATH=$(realpath "$0/../..")

rm -rf "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/"
parcel build --dist-dir "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/" --no-source-maps --no-optimize "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/src/main.html"

bundled_js_filename=$(ls "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist" | grep '.*\.js')
bundled_js_contents=$(cat "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/$bundled_js_filename")

export script_tag_to_replace='<script type="module" src="/'$bundled_js_filename'"></script>'
export script_tag_to_insert='<script type="module">'$bundled_js_contents'</script>'

<"$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/main.html" awk '
    s=index($0,ENVIRON["script_tag_to_replace"]){$0=substr($0, 1, s-1) ENVIRON["script_tag_to_insert"] substr($0, s + length(ENVIRON["script_tag_to_replace"]))} 1
' > "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/wifi-gateway-ui.html"

"$REPOSITORY_BASE_PATH/tools/generate-embedded-html.sh" "$REPOSITORY_BASE_PATH/src/wifi-gateway-ui/dist/wifi-gateway-ui.html"
