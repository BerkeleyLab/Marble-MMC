# Configure marble_mmc IP and mac addresses based on serial number
# config.sh serial_number

if [ $# -lt 1 ]; then
  if [[ ! ( -z $IP ) ]]; then
    snum=${IP[@]: -2}
    echo "Using serial number from IP: $snum"
  else
    echo "Usage: config.sh serial_number"
    exit 1
  fi
else
  snum=$1
fi

printf -v mac "12:55:55:0:1:%x" "$snum"
printf -v ip "192.168.19.%s" "$snum"

echo "ip = $ip; mac = $mac"

# TODO - figure out how to handle using different ttyUSB numbers
python3 load.py "m $ip" "n $mac"
