#!/usr/bin/env bash

thumb_dir="${XDG_CACHE_HOME:-$HOME/.cache}/cliphist/thumbs"
mkdir -p "$thumb_dir"

cliphist_list=$(cliphist list)

declare -A existing_ids
while IFS=$'\t' read -r id _; do
    [[ -n "$id" ]] && existing_ids["$id"]=1
done <<< "$cliphist_list"

for thumb_path in "$thumb_dir"/*; do
    [[ -e "$thumb_path" ]] || continue
    thumb_id="${thumb_path##*/}"
    thumb_id="${thumb_id%.*}"
    
    if [[ -z "${existing_ids[$thumb_id]}" ]]; then
        rm -f "$thumb_path"
    fi
done

processed_list=$(awk -v thumb_dir="$thumb_dir" '
    /^[0-9]+\s<meta http-equiv=/ { next }  # Skip meta tags
    match($1, /^([0-9]+)\s+(\[\[\s)?binary.*(jpe?g|png|bmp|gif|webp)/, m) {
        id = m[1]
        ext = m[3]
        thumb_file = thumb_dir "/" id "." ext
        
        # Generate thumbnail if needed
        if (!system(" [ -f \"" thumb_file "\" ] ")) {
            print id
            print "img:" thumb_file
            next
        }
        
        # Try to generate thumbnail
        decode_cmd = "printf \"" id "\\t\" | cliphist decode"
        convert_cmd = "magick - -resize 256x256> -strip -quiet \"" thumb_file "\" 2>/dev/null"
        
        if (system(decode_cmd " | " convert_cmd) == 0) {
            print id
            print "img:" thumb_file
            next
        }
        
        # Fallback to text if conversion fails
        rm -f thumb_file
    }
    { print }  # Print non-image entries as-is
' <<< "$cliphist_list")

choice=$(wofi -I --dmenu \
              -Dimage_size=200 \
              -Dcontent_halign=center \
              -Dline_wrap=word_char \
              -Dwidth=40% \
              -Ddynamic_lines=true \ \
              -p "Clipboard" \
              -k /dev/null <<< "$processed_list")

[[ -z "$choice" ]] && exit 0

if [[ "$choice" == img:* ]]; then
    notify-send "Clipboard" "Item cannot be copied (select the ID instead)" -i dialog-error
    exit 1
fi

clip_id=$(echo "$choice" | awk 'NR==1 {print $1}')

printf "%s\t" "$clip_id" | cliphist decode | wl-copy

notify-send "Clipboard" "Item copied to clipboard" -i edit-copy
