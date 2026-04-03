#!/bin/bash
#
# newrepo.bash
# Hybrid RobotiX — UNO Q Environment Bootstrap
#
# Clones the UNO-Q repo fresh, copies Arduino apps and bin commands
# to $HOME, copies secrets to each app that needs them, and creates
# versioned symlinks for all bin commands.
#
# This script lives in $HOME and is NEVER stored in the repo.
# Copy it manually to $HOME on any new UNO Q for the first run:
#   cp ~/Repos/GitHub/hybotix/UNO-Q/scripts/newrepo.bash ~/newrepo.bash
#
# After the first run, the start command installs ~/bin/newrepo automatically.
#
# Usage:
#   bash ~/newrepo.bash   # first time
#   newrepo               # after first start
#

REPO_DEST="$HOME/Repos/GitHub/hybotix/UNO-Q"
REPO="https://github.com/hybotix/UNO-Q.git"
SECRETS_DEST="securesmars"
COMMANDS="addlib build clean list logs restart start stop"

cd $HOME
rm -rf Arduino bin Repos
git clone $REPO $REPO_DEST
cd $REPO_DEST
cp -rp Arduino bin $HOME

#
#   Copy secrets.py.template to app directories
#
cd $HOME
for dest in $SECRETS_DEST; do
    cp secrets.py.template Arduino/$dest/python/secrets.py
    echo "Secrets: copied to Arduino/$dest/python/secrets.py"
done

#
# Make the symbolic links to the latest version of each command
#
cd $HOME/bin
for cmd in $COMMANDS; do
    latest=$(ls ${cmd}-v*.py 2>/dev/null | sort -V | tail -1)
    if [ -n "$latest" ]; then
        ln -sf $HOME/bin/$latest $HOME/bin/$cmd
        chmod +x $HOME/bin/$cmd
        echo "Linked: $cmd -> $latest"
    else
        echo "WARNING: No versioned file found for $cmd"
    fi
done
