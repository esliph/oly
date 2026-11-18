#!/bin/sh
# Puts the oly launcher in $OLY_HOME/bin and that folder on the PATH of the shell profile. Run as:
#   curl -fsSL https://github.com/esliph/oly/releases/latest/download/install.sh | sh
# OLY_INSTALL_VERSION installs a release other than the latest.

oly_home=${OLY_HOME:-$HOME/.oly}
release_url=${OLY_RELEASE_URL:-https://github.com/esliph/oly/releases}

if [ -n "${OLY_INSTALL_VERSION:-}" ]; then
  url=$release_url/download/v$OLY_INSTALL_VERSION/oly
else
  url=$release_url/latest/download/oly
fi

mkdir -p "$oly_home/bin" || exit 1
temporary=$oly_home/bin/oly.new.$$
# A download cut short can still parse, so the closing line of the launcher is required too.
if ! curl -fsSL "$url" -o "$temporary" || ! sh -n "$temporary" || [ "$(tail -n 1 "$temporary")" != "# end of oly" ]; then
  rm -f "$temporary"
  printf 'install.sh: could not download a complete launcher from %s\n' "$url" >&2
  exit 1
fi
chmod +x "$temporary" && mv "$temporary" "$oly_home/bin/oly" || exit 1

case ${SHELL:-} in
  *zsh) profile=$HOME/.zshrc ;;
  *) profile=$HOME/.bashrc ;;
esac

path_line="export PATH=\"$oly_home/bin:\$PATH\""
if ! grep -qsF "$oly_home/bin" "$profile"; then
  printf '%s\n' "$path_line" >> "$profile"
fi

printf 'oly installed in %s; open a new terminal or run: %s\n' "$oly_home/bin/oly" "$path_line"
