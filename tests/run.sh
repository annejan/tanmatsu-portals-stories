#!/bin/sh
# The plugin's fetching and card writing, on the host (tests/host.c): new
# packs, an update that drops and adds files, a pack already on the card,
# and "get all". Usage: tests/run.sh <the compiled host test>
set -e
bin=$(realpath "$1")
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp -r "$here/site" "$work/site"
mkdir -p "$work/card"
cd "$work"
fail() { echo "FAIL: $*"; exit 1; }
status() { "$bin" "$1" | grep -E '^[a-z]+: ' > status.txt; grep -qx "$2" status.txt || { cat status.txt; fail "want '$2'"; }; }

# Nothing on the card: both new.
status x "alpha: new"
status x "beta: new"

# Enter on the first: alpha arrives, as on the site, and is stamped.
status ex "alpha: up to date"
diff -r --exclude=files.txt site/packs/alpha card/alpha --exclude=.stories || fail "alpha differs from the site"
head -1 card/alpha/.stories | grep -q '^e0a657e6' || fail "no stamp"
status x "beta: new"

# The site changes alpha: desk/mail.txt goes, two.txt comes, one.txt changes.
rm site/packs/alpha/desk/mail.txt
printf 'name: Two\n' > site/packs/alpha/two.txt
printf 'name: One, again\n' > site/packs/alpha/one.txt
printf '17\tone.txt\n25\tpack.txt\n10\ttwo.txt\n' > site/packs/alpha/files.txt
sed -i 's/^alpha\te0a657e6[0-9a-f]*/alpha\t1111111111111111111111111111111111111111111111111111111111111111/' site/index.txt
status x "alpha: update"
status ex "alpha: up to date"
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
