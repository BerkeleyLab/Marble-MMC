# Segger VID = 0x1366; ST VID = 0x0483
case "$OSTYPE" in
    linux-gnu*)
        CMD="lsusb"
        ;;
    darwin*)
        CMD="system_profiler SPUSBDataType"
        ;;
esac

if $CMD | grep -q 1366; then
    DEBUG_ADAPTER=segger;
elif $CMD | grep -q 0483; then
	DEBUG_ADAPTER=st;
fi
#if [ -n "$( lsusb | grep 1366 )" ]; then
  #DEBUG_ADAPTER=segger;
#elif [ -n "$( lsusb | grep 0483 )" ]; then
  #DEBUG_ADAPTER=st;
#fi
export DEBUG_ADAPTER=$DEBUG_ADAPTER
echo $DEBUG_ADAPTER
