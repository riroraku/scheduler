NOTES ON PROGRAM USE:
DO NOT USE THIS PROGRAM IN PRODUCTION! IT IS ONLY INTENDED 
FOR PERSONAL USE AND FOR CLARIFICATION I DID TEST ONLY ON
TERMUX (NO ROOT). IF YOU TRY TO USE, TO RUN SCRIPT FROM FULL
FLEDGE SYSTEM THIS PROGRAM MIGHT NOT RUN IN SOME CASES.
WHEN I SAY PERSONAL, I MEAN YOUR PERSONAL PROJECTS OR YOUR
PERSONAL USER IN THE SYSTEM (RUN ONLY YOUR OWNED SCRIPTS).

DESCRIPTION
This is a small task scheduler intended to run scripts on loop! 
This program consists of (2) args:
		scheduler [minute] [script_file]

		[minute]       Run script file  for every 1 - 1440 minutes.
		[script_file]  A script file to run.

To run program you can either run it on background with (&)
		(e.g.) ./scheduler 60 log_users &
or foreground
		(e.g.) ./scheduler 60 log_users

(Note: All fd (except stdou) are redirected to /dev/null)

COMPILATION
Just compile it normally! Copy and paste then:
```
	gcc scheduler.c -o scheduler
```
then run!
or run the script
```
	curl -sS "https://raw.githubusercontent.com/riroraku/scheduler/refs/heads/main/compile.sh" -o compile.sh && chmod 700 compile.sh && ./compile.sh
```
if you are truly a beautiful human being you can clone it then run:
```
	./compile.sh
```

PROGRAM LIMITATION
Due to Android's Power policy when device screen is off, the kernel
limits CPU usage so it make the programs runs on very low priority.
For temporary solution whenever your device is screen off restart the
daemon process by executing
```
	kill -SIGHUP <PID>
```
where the PID is the program's PID
You can get the program's PID by executing
```
	ps x | grep "scheduler"
```

TEST ENVIRONMENT/MACHINE
Linux 5.10.209-android12-9-00016-g7c6bbcca33e1-ab12029497 #1
SMP PREEMPT Fri Jun 28 13:58:53 UTC 2024 aarch64 Android 13

TERMUX VERSION
0.118.3 (non rooted)

GCC 
clang version 21.1.8
Target: aarch64-unknown-linux-android24
Thread model: posix
InstalledDir: /data/data/com.termux/files/usr/bin

Q&A
Q. Why using system() for beep character?
A. I tried using printf() for that character it seems doesnt
work so no choice i used system() calling echo -ne "\a".

NOT RELATED
I program this one on phone with nano! Though theres a decent
on mobile phones but it seem i really gotten hang-up on nano
especially when i discovered nano could be configured!
