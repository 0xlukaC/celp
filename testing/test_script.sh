#!/usr/bin/env bash
set -eu

function testFile() {
    local path="$1"
    local expected_status=${2:-"null"}
    local expected_content_type=${3:-"null"}
    local text=${4:-"null"}
    local url="http://localhost:8001/$path"
    echo "" 
    echo "Testing $url w/ $expected_status & $expected_content_type..."

    # Fetch response headers
    # D dumps the header
    local headers=$(curl -sD - http://localhost:8001/example.txt)
    # echo "$headers"
    
    if [[ "$text" != "null" ]]; then
        if ! echo "$headers" | greq -iq "$text"; then
            return 1
        fi
    fi
    
    # Check the status code
    if [[ "$expected_status" != "null" ]] then 
        if ! echo "$headers" | grep -q "HTTP/1.1 $expected_status"; then
            echo "Test failed: Expected status '$expected_status', got something else."
            return 1
        fi
    fi

    # Check the content type
    local actual_content_type=$(echo "$headers" | grep -i "Content-Type" | awk '{print $2}' | tr -d '\r')
    if [[ "$expected_content_type" != "null" && "$actual_content_type" != "$expected_content_type" ]]; then
        echo "Test failed: Expected content type '$expected_content_type', got '$actual_content_type'."
        return 1
    fi


    # Compare the response with the expected file
    if [[ -f "$path" ]]; then
        tempfile=$(mktemp)
        trap "rm -f $tempfile" EXIT
        curl -s "$url" > $tempfile

        diff "$tempfile" "$path" > /dev/null
        if [[ $? -ne 0 ]]; then
            echo "Test failed: Response does not match expected output!"
            return 1
        fi
    fi

    echo ""
    return 0
}

# Example usage
# testFile "example.html" "404" "text/html"
# testFile "asdf* ///gArBage/" "404"
# testFile "public/image.jpg" "200"
testFile ""

# Test POST
function testPost() {
    local path="$1"
    local text=${2:-"null"}
    
    if [[ "$text" != "null" ]]; then 
        if ! curl -X POST "$path" 2> /dev/null | grep -iq "$text"; then
            echo "Test failed: Text '$text' not found in response."
            return 1
        fi
    elif ! curl -X POST "$path" -f -s; then
        return 1
        echo "Test failed: POST request to '$path' failed."
    fi
    return 0
}

testPost "http://localhost:8001/login" "Login text"
