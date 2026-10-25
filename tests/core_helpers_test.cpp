#include "utils/release_version.hpp"
#include "utils/url_encode.hpp"
#include <cassert>
#include "utils/xmltv_time.hpp"

int main() {
    using tsvitch::isNewerStableRelease;
    using tsvitch::xmltvTimestamp;
    assert(xmltvTimestamp("20261025000000 +0000") == 1792886400);
    assert(xmltvTimestamp("20261025020000 +0200") == 1792886400);
    assert(xmltvTimestamp("20261024200000 -0400") == 1792886400);
    assert(xmltvTimestamp("20261025000000") == 1792886400);
    assert(xmltvTimestamp("20240229010000 +0000") > 0);
    for (const auto& value : {"", "20261025", "20260229010000 +0000", "20261325000000 +0000", "20261025000000 +2360", "20261025000000 X0200"})
        assert(xmltvTimestamp(value) == 0);

    assert(isNewerStableRelease("v1.0.0", {0, 9, 9}));
    assert(isNewerStableRelease("0.4.0", {0, 3, 9}));
    assert(isNewerStableRelease("v0.3.2", {0, 3, 1}));
    assert(!isNewerStableRelease("v0.2.99", {0, 3, 1}));
    assert(!isNewerStableRelease("v0.99.99", {1, 0, 0}));
    assert(!isNewerStableRelease("v0.3.1", {0, 3, 1}));
    for (const auto& invalid : {"", "v", "1.2", "1.2.3.4", "-1.2.3", "1.x.3", "1.2.3-beta", "01.2.3", "999999999999.1.1"})
        assert(!isNewerStableRelease(invalid, {0, 3, 1}));
    assert(tsvitch::encodeUrlComponent("abc-_.~09") == "abc-_.~09");
    assert(tsvitch::encodeUrlComponent("a&b=c ?/#%+") == "a%26b%3Dc%20%3F%2F%23%25%2B");
    assert(tsvitch::encodeUrlComponent("שלום") == "%D7%A9%D7%9C%D7%95%D7%9D");
}
