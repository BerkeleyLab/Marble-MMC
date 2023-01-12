# Segger VID = 0x1366; ST VID = 0x0483
if [ -n "$( lsusb | grep 1366 )" ]; then
  DEBUG_ADAPTER=segger;
elif [ -n "$( lsusb | grep 0483 )" ]; then
  DEBUG_ADAPTER=st;
fi
export DEBUG_ADAPTER=$DEBUG_ADAPTER
echo $DEBUG_ADAPTER
