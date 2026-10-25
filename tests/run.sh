#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TEST_DIR=$(mktemp -d)
trap 'rm -rf "$TEST_DIR"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include tests/core_helpers_test.cpp -o "$TEST_DIR/helpers"
"$TEST_DIR/helpers"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/media_compatibility_test.cpp -o "$TEST_DIR/media"
"$TEST_DIR/media"
python3 tests/bidi_test.py
c++ -std=c++17 -Wall -Wextra -Werror -I tests/stubs -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/channel_cache_test.cpp tsvitch/source/core/ChannelManager.cpp -o "$TEST_DIR/cache"
"$TEST_DIR/cache"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/playlist_catchup_test.cpp -o "$TEST_DIR/playlist"
"$TEST_DIR/playlist"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/watch_store_test.cpp -o "$TEST_DIR/watch"
"$TEST_DIR/watch"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include tests/update_files_test.cpp -o "$TEST_DIR/update"
"$TEST_DIR/update" "$TEST_DIR/update-files"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/library_features_test.cpp -o "$TEST_DIR/library"
"$TEST_DIR/library" "$TEST_DIR/library-data"
c++ -std=c++17 -Wall -Wextra -Werror -I tsvitch/include -I tsvitch/include/api -I library/nlohmann_json/single_include tests/content_navigation_test.cpp -o "$TEST_DIR/content"
"$TEST_DIR/content"
if pkg-config --exists fribidi 2>/dev/null; then
    cc -std=c99 -Wall -Wextra -Werror $(pkg-config --cflags fribidi) tests/bidi_cursor_test.c $(pkg-config --libs fribidi) -o "$TEST_DIR/bidi-cursor"
    "$TEST_DIR/bidi-cursor"
fi
python3 tests/font_metadata_test.py
