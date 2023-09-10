#!/bin/sh

script_base_path=$(realpath "$0/..")

input_file=$1 #e.g. src/test/test-ui.html
input_name=$(basename "$input_file" .html) # -> test-ui
input_dir=$(dirname "$input_file") # -> src/test

output_file=$(echo "$input_dir"/"$input_name".generated.h) # -> src/test/test-ui.generated.h
export namespace=$(echo $input_name | tr '.- ' '_') # -> test_ui
export include_guard=$(echo "$namespace"_H | tr '[:lower:]' '[:upper:]') # -> TEST_UI_H

# generate gzip'ed HTML as C/C++ compatible list of bytes
export bytes=$(cat "$input_file" | gzip | xxd -i)
export bytes_size=$(cat "$input_file" | gzip | wc -c)

# generate header from template
<"$script_base_path/embedded-html.h.in" awk '
    {
    gsub(/__INCLUDE_GUARD__/, ENVIRON["include_guard"]);
    gsub(/__namespace__/, ENVIRON["namespace"]);
    gsub(/\/\*BYTES\*\//, ENVIRON["bytes"]);
    gsub(/\/\*BYTES_SIZE\*\//, ENVIRON["bytes_size"]);
    print;
    }
' > "$output_file"
