#! /bin/bash

echo "======================================================================="
echo "Log update script for mooproxy"
echo
echo "In mooproxy 0.1.2, the location of logfiles has changed. This script is"
echo "intended for users upgrading from mooproxy 0.1.1 (or older) to 0.1.2."
echo "It will move the old logfiles to their new destinations."
echo
echo "Mooproxy < 0.1.2 logs files like this:"
echo "  logs/worldname - YYYY-MM-DD.log"
echo
echo "Mooproxy >= 0.1.2 logs files to a nested hierarchy, like this:"
echo "  logs/worldname/YYYY-MM/worldname - YYYY-MM-DD.log"
echo
echo "This script will create the appropriate directories, and move any"
echo "existing logfiles to their correct location."
echo
echo "This script assumes your logs are in ~/.mooproxy/logs/."
echo "Also, just in case:"
echo
echo "    >>  MAKE BACKUPS OF YOUR LOGS BEFORE RUNNING THIS SCRIPT.  <<"
echo "======================================================================="
echo
echo "To continue, please type the phrase 'Yes, move my logs.'"

echo -n "> "
read CONFIRMATION

if [ "${CONFIRMATION}" != "Yes, move my logs." ]
then
	echo "Aborting."
	exit 0
fi

echo
echo "Moving logs..."
echo
echo

cd ~/.mooproxy/logs ||
	(echo "cd ~/.mooproxy/logs failed!"; exit 1)

NOTMOVED=""

for I in *.log
do
	WORLD=$(echo "${I}" | sed 's/ - ....-..-..\.log$//')
	MONTH=$(echo "${I}" | sed 's/-..\.log$//' | grep -oe '....-..$')
	if [ ! -d "${WORLD}" ]
	then
		echo "mkdir ${WORLD}/"
		mkdir "${WORLD}" ||
			(echo "mkdir ${WORLD} failed!"; exit 1)
	fi
	if [ ! -d "${WORLD}/${MONTH}" ]
	then
		echo "mkdir ${WORLD}/${MONTH}/"
		mkdir "${WORLD}/${MONTH}" ||
			(echo "mkdir ${WORLD}/${MONTH} failed!"; exit 1)
	fi

	if [ -e "${WORLD}/${MONTH}/${I}" ]
	then
		echo "!   ${WORLD}/${MONTH}/${I} already exists, not moving."
		NOTMOVED="true"
	else
		echo -n "    "
		mv -iv "${I}" "${WORLD}/${MONTH}/" ||
			(echo "mv -iv ${I} ${WORLD}/${MONTH}/ failed!"; exit 1)
	fi
done

echo
echo "========="
echo "All done."

if [ -n "${NOTMOVED}" ]
then
	echo
	echo "The following filenames already existed in their new location,"
	echo "and were therefore not moved:"
	echo
	ls -1 *.log
	echo
	echo "Please merge these files manually."
else
	echo "All files were moved."
fi
