#!/bin/sh
# The plugin's fetching and card writing, on the host (tests/host.c): new
# packs, an update that drops and adds files, a pack already on the card,
# and "get all"; a pack in one request (packs/<id>.txt), or file by file
# where the site has no such bundle or a stale one. Usage: tests/run.sh <the compiled host test>
set -e
bin=$(realpath "$1")
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp -r "$here/site" "$work/site"
mkdir -p "$work/card"
cd "$work"
fail() { echo "FAIL: $*"; exit 1; }
bundle() { (cd "site/packs/$1" && cut -f2 files.txt | xargs cat) > "site/packs/$1.txt"; }
gets() { "$bin" "$1" | grep -c '^  > packs/'; }  # requests for pack files
status() { "$bin" "$1" | grep -E '^[a-z]+: ' > status.txt; grep -qx "$2" status.txt || { cat status.txt; fail "want '$2'"; }; }

# Nothing on the card: both new.
status x "alpha: new"
status x "beta: new"

# Enter on the first: alpha arrives, as on the site, and is stamped. One
# request for the list, one for the bundle.
[ "$(gets ex)" = 2 ] || fail "alpha not in one request"
rm -rf card/alpha
status ex "alpha: up to date"
diff -r --exclude=files.txt site/packs/alpha card/alpha --exclude=.stories || fail "alpha differs from the site"
head -1 card/alpha/.stories | grep -q '^e0a657e6' || fail "no stamp"
status x "beta: new"

# The site changes alpha: desk/mail.txt goes, two.txt comes, one.txt changes.
rm site/packs/alpha/desk/mail.txt && rmdir site/packs/alpha/desk
printf 'name: Two\n' > site/packs/alpha/two.txt
printf 'name: One, again\n' > site/packs/alpha/one.txt
printf '17\tone.txt\n25\tpack.txt\n10\ttwo.txt\n' > site/packs/alpha/files.txt
sed -i 's/^alpha\te0a657e6[0-9a-f]*/alpha\t1111111111111111111111111111111111111111111111111111111111111111/' site/index.txt
status x "alpha: update"
# alpha.txt is the old one now: the wrong size, so file by file after it
# (list, bundle, 3 files).
[ "$(gets ex)" = 5 ] || fail "a stale bundle was used"
status x "alpha: up to date"
[ ! -e card/alpha/desk/mail.txt ] || fail "a file the site dropped is still on the card"
grep -q 'again' card/alpha/one.txt || fail "one.txt not updated"
[ -e card/alpha/two.txt ] || fail "two.txt not added"
# The same from a fresh bundle.
bundle alpha
rm -rf card/alpha
[ "$(gets ex)" = 2 ] || fail "the fresh bundle was not used"
diff -r --exclude=files.txt site/packs/alpha card/alpha --exclude=.stories || fail "alpha from the bundle differs"
status x "alpha: up to date"
[ ! -e card/alpha/desk/mail.txt ] || fail "a file the site dropped is still on the card"
grep -q 'again' card/alpha/one.txt || fail "one.txt not updated"
[ -e card/alpha/two.txt ] || fail "two.txt not added"

# A pack put on the card some other way (a zip, USB): on the card, not stamped.
mkdir -p card/beta && printf 'name: Beta\n' > card/beta/pack.txt
status x "beta: on the card"

# A file of the wrong size: refused, the card as it was.
printf '99\tpack.txt\n5\ttwo.txt\n' > site/packs/beta/files.txt
status dex "beta: on the card"
[ ! -e card/beta/two.txt ] || fail "half a pack written"

# "a": every new and updated pack -- none left now but beta, which is not new.
rm -rf card/alpha
status ax "alpha: up to date"
echo "all host tests pass"
