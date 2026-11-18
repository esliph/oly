# Runs every *_test.sh of this directory; fails when any of them does.
directory=$(cd "$(dirname "$0")" && pwd)
failed=0

for test in "$directory"/*_test.sh; do
  printf '== %s\n' "${test##*/}"
  sh "$test" || failed=1
done

exit "$failed"
