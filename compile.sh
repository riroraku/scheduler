#!/data/data/com.termux/files/usr/bin/bash
REPO_FILE="https://raw.githubusercontent.com/riroraku/scheduler/refs/heads/main/scheduler.c"
# folks respect license
REPO_LICENSE="https://raw.githubusercontent.com/riroraku/scheduler/refs/heads/main/LICENSE"
strip_flag=0

__fetch() {
	local status=$(curl -sSw "%{http_code}" "$1" -o /dev/null)
	if [ "$?" -eq 0 ]; then
		if [ "$status" -eq 200 ]; then
			curl -sS "$1" -o "./$2"
			return 0
		else
			return 1
		fi
	fi
	return 1
}

echo "Checking gcc if installed..."
sleep 0.5
! command -v gcc &> /dev/null && echo 'Please install gcc first by running `pkg update && pkg install gcc -y`!' && exit 1
echo "Installed!"
sleep 1
echo "Checking strip if installed..."
sleep 0.5
if ! command -v strip &> /dev/null; then
	strip_flag=1
	echo "No stripping"
else
	echo "Installed!"
fi
sleep 1
echo "Fetching scheduler.c from repo..."
sleep 0.5
! __fetch "$REPO_FILE" "scheduler.c" && echo "Unable to fetch $REPO_FILE. Exit." && exit 1
echo "Fetched!"
sleep 1
echo "Fetching LICENSE from repo for a copy..."
sleep 0.5
! __fetch "$REPO_LICENSE" "LICENSE" && echo -e "\e[5;41mPlease dont forget to have a copy of the GNU GPLv2 LICENSE.\e[0m"
echo "Fetched!"
sleep 1
echo "Compilation"
sleep 1
gcc --version
sleep 1
echo "Compiling..."
gcc -Wall "./scheduler.c" -o "./scheduler"
echo "Compiled!"
sleep 1

if [ "$strip_flag" -eq 0 ]; then
	echo "Stripping file..."
	strip --strip-all --strip-section-headers "./scheduler"
	sleep 0.5
	echo "Done stripping!"
else
	echo "Skip stripping."
fi
sleep 0.5
echo "Done! Bin file located in $(pwd)/scheduler!"
