if which apt-get > /dev/null 2>&1; then LOCK_FILE="/var/lib/dpkg/lock-frontend";\
elif which dnf > /dev/null 2>&1; then LOCK_FILE="/var/lib/rpm/.rpm.lock";\
elif which yum > /dev/null 2>&1; then LOCK_FILE="/var/lib/rpm/.rpm.lock";\
elif which zypper > /dev/null 2>&1; then LOCK_FILE="/var/lib/rpm/.rpm.lock";\
elif which pacman > /dev/null 2>&1; then LOCK_FILE="/var/lib/pacman/db.lck";\
else echo "Packet manager not found"; echo "Internal error"; exit 1; fi;\
if command -v fuser > /dev/null 2>&1; then sudo fuser $LOCK_FILE 2>/dev/null; else echo "fuser not installed"; fi
