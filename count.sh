#!/bin/bash

# --- Configuration ---
OUTPUT_FILE="count.md"

# Define common extensions for files to EXCLUDE from the main character/line count.
EXCLUDE_EXTENSIONS=("jpg" "jpeg" "png" "gif" "bmp" "ico" "mp3" "mp4" "mov" "avi" "zip" "rar" "gz" "tar" "bin" "exe" "dll" "so" "o" "class" "pdf" "doc" "docx" "xls" "xlsx")

# Define common extensions for files you want to explicitly list in the breakdown table
# All other code/text files will be grouped under "Other Code/Text"
KNOWN_CODE_EXTENSIONS=("sh" "bash" "py" "js" "ts" "html" "css" "scss" "php" "c" "cpp" "h" "java" "go" "md" "txt" "yaml" "json" "xml")


# Convert the exclusion array to a regex pattern for use with 'find -not -regex'
EXCLUDE_PATTERN=$(printf "\.%s\\|" "${EXCLUDE_EXTENSIONS[@]}" | sed 's/|$/$/')
EXCLUDE_PATTERN="${EXCLUDE_PATTERN/\\|/\|}"
EXCLUDE_PATTERN=".*($EXCLUDE_PATTERN)"

# Start by clearing the output file
echo "# 📁 Directory Content Analysis" > "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"
echo "Analysis started from: \`$(pwd)\`" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# --- 1. Total Code/Text Lines and Characters Count (Unchanged) ---

echo "## 💻 Code/Text Lines & Characters" >> "$OUTPUT_FILE"
echo "*(Excluding files with extensions: ${EXCLUDE_EXTENSIONS[*]::$(( ${#EXCLUDE_EXTENSIONS[@]} > 5 ? 5 : ${#EXCLUDE_EXTENSIONS[@]} ))}[...] for brevity)*" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

find . -type f -not -regex "$EXCLUDE_PATTERN" -print0 | xargs -0 wc -lcm | awk '
BEGIN {
    lines = 0;
    chars = 0;
}
END {
    if (NF >= 3) {
        lines = $1;
        chars = $2;
    }
    print "| Metric | Count |"
    print "| :--- | :--- |"
    print "| **Total Lines** | " lines " |"
    print "| **Total Characters** | " chars " |"
}' >> "$OUTPUT_FILE"

echo "" >> "$OUTPUT_FILE"

# --- 2. File Classification and Summary Count (Updated Logic) ---

declare -A EXTENSION_COUNTS
CODE_COUNT=0
BINARY_COUNT=0
TOTAL_FILES=0
OTHER_CODE_COUNT=0

# Loop through all regular files to classify and count them
while IFS= read -r FILE; do
    # Get the file's extension, defaulting to "None" if no extension is found
    EXTENSION=$(echo "$FILE" | awk -F'.' '{if (NF>1) {print $NF} else {print "None"}}' | tr '[:upper:]' '[:lower:]')

    # Check if the extension is considered Binary/Media
    IS_BINARY=0
    for EXT in "${EXCLUDE_EXTENSIONS[@]}"; do
        if [[ "$EXTENSION" == "$EXT" ]]; then
            IS_BINARY=1
            break
        fi
    done

    # Count based on classification for the top-level summary
    if [[ $IS_BINARY -eq 1 ]]; then
        ((BINARY_COUNT++))
        # Log binary files by their extension
        EXTENSION_COUNTS["**Binary/Media**"]=$((${EXTENSION_COUNTS["**Binary/Media**"]:-0} + 1))
    else
        ((CODE_COUNT++))
        
        # Check if the code/text extension is one we want to explicitly list
        IS_KNOWN_CODE=0
        for EXT in "${KNOWN_CODE_EXTENSIONS[@]}"; do
            if [[ "$EXTENSION" == "$EXT" ]]; then
                IS_KNOWN_CODE=1
                break
            fi
        done
        
        # Group Code/Text files: if known, list extension; if not, group as "Other Code/Text"
        if [[ $IS_KNOWN_CODE -eq 1 ]]; then
            EXTENSION_COUNTS["$EXTENSION"]=$((${EXTENSION_COUNTS["$EXTENSION"]:-0} + 1))
        elif [[ "$EXTENSION" == "None" ]]; then
            # Files with no extension are still tracked under "None"
            EXTENSION_COUNTS["None"]=$((${EXTENSION_COUNTS["None"]:-0} + 1))
        else
            # All other (less common) code/text files are grouped here
            ((OTHER_CODE_COUNT++))
        fi
    fi

    ((TOTAL_FILES++))

done < <(find . -type f)

# Add the cumulative "Other Code/Text" count to the tracking array if greater than zero
if [[ $OTHER_CODE_COUNT -gt 0 ]]; then
    EXTENSION_COUNTS["**Other Code/Text**"]=$OTHER_CODE_COUNT
fi


# --- 3. Output Top-Level Summary Table (Unchanged) ---

echo "## 📊 File Type Summary" >> "$OUTPUT_FILE"
echo "| File Type | Count | Percentage |" >> "$OUTPUT_FILE"
echo "| :--- | :--- | :--- |" >> "$OUTPUT_FILE"

# Calculate percentages
if [[ $TOTAL_FILES -gt 0 ]]; then
    CODE_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($CODE_COUNT/$TOTAL_FILES)*100}")
    BINARY_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($BINARY_COUNT/$TOTAL_FILES)*100}")
else
    CODE_PERCENT=0.0
    BINARY_PERCENT=0.0
fi

echo "| **Code/Text Files** | $CODE_COUNT | $CODE_PERCENT% |" >> "$OUTPUT_FILE"
echo "| Binary/Media Files | $BINARY_COUNT | $BINARY_PERCENT% |" >> "$OUTPUT_FILE"
echo "| **TOTAL** | **$TOTAL_FILES** | **100.0%** |" >> "$OUTPUT_FILE"

echo "" >> "$OUTPUT_FILE"

# --- 4. Output Extension Breakdown Table (Updated Logic) ---

echo "## 🔍 File Count by Extension Breakdown" >> "$OUTPUT_FILE"
echo "| Extension | Count | Percentage |" >> "$OUTPUT_FILE"
echo "| :--- | :--- | :--- |" >> "$OUTPUT_FILE"

# Sort the extensions by count in descending order
for EXTENSION in "${!EXTENSION_COUNTS[@]}"; do
    echo "$EXTENSION ${EXTENSION_COUNTS[$EXTENSION]}"
done | sort -nr -k2 | while read EXTENSION COUNT; do
    PERCENTAGE=$(awk "BEGIN {printf \"%.1f\", ($COUNT/$TOTAL_FILES)*100}")
    
    # Custom display logic for grouping headers
    if [[ "$EXTENSION" == "**Binary/Media**" || "$EXTENSION" == "**Other Code/Text**" ]]; then
        echo "| $EXTENSION | $COUNT | $PERCENTAGE% |" >> "$OUTPUT_FILE"
    elif [[ "$EXTENSION" == "None" ]]; then
        echo "| **$EXTENSION** | $COUNT | $PERCENTAGE% |" >> "$OUTPUT_FILE"
    else
        echo "| .$EXTENSION | $COUNT | $PERCENTAGE% |" >> "$OUTPUT_FILE"
    fi
done

echo "" >> "$OUTPUT_FILE"
echo "---" >> "$OUTPUT_FILE"
echo "Analysis complete. Output saved to \`$OUTPUT_FILE\`."