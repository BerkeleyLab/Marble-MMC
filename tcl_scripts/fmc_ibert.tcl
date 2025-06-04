# Connect to the Digilent Cable on localhost:3121
open_hw_manager
connect_hw_server -url localhost:3121 -verbose
# current_hw_target [get_hw_targets */digilent_plugin/SN:12345]
open_hw_target

# Program and Refresh the xc7k160tffg676-2
current_hw_device [lindex [get_hw_devices] 0]
refresh_hw_device -update_hw_probes false [lindex [get_hw_devices] 0]
puts "Programming marble_ibert.bit....."
set_property PROGRAM.FILE {marble_ibert.bit} [lindex [get_hw_devices] 0]
program_hw_devices [lindex [get_hw_devices] 0]
refresh_hw_device [lindex [get_hw_devices] 0]

exec sleep 5
unset env(PYTHONPATH)
unset env(PYTHONHOME)
set MMC_PATH $env(MMC_PATH)
puts "Changing GTX routing to have 2 channels on FMC P2 and 2 channels on FMC P1....."
exec python3 $MMC_PATH/scripts/load.py -d /dev/ttyUSB3 “r off”
exec $MMC_PATH/scripts/mgtmux.sh -d /dev/ttyUSB3 M1

puts "Sleep for 10 seconds...."
exec sleep 10

# QSFP2
set xil_newLinks [list]
set xil_newLink [create_hw_sio_link -description {Link 0} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y0/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y0/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 1} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y1/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y1/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 2} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y2/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y2/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 3} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y3/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y3/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLinkGroup [create_hw_sio_linkgroup -description {Link Group 0} [get_hw_sio_links $xil_newLinks]]

puts "Sleep for 20 seconds...."
exec sleep 20
set errors 0
set thres 1e-11
set error_links []
refresh_hw_sio [get_hw_sio_links $xil_newLinks]
for {set y 0} {$y<1} {incr y} {
    puts "#####################################"
    puts "FMC2 + FMC1"
    puts "#####################################"
    for {set x 0} {$x<4} {incr x} {
        puts "Link $x status"
        set l [expr $x + 4 * $y]
        set status [get_property STATUS [lindex [get_hw_sio_links] $l]]
        report_property -pattern STATUS [lindex [get_hw_sio_links] $l]
        if {[string match "NO LINK" $status]} {
            incr errors
            lappend error_links "FMC2 + FMC1 Link $x (NO LINK)"
        } else {
            if {[string match "10.000 Gbps" $status]} {
                set ber [get_property RX_BER [lindex [get_hw_sio_links] $l]]
                report_property -pattern RX_BER [lindex [get_hw_sio_links] $l]
                if {[expr $ber > $thres]} {
                    incr errors
                    lappend error_links "FMC2 + FMC1 Link $x (RX_BER > $thres)"
                }
            } else {
                incr errors
                lappend error_links "FMC2 + FMC1 Link $x (Speed not 10.000 Gbps)"
            }
        }
        puts "*************************************"
     }
}

if {$errors > 0} {
    error "Errors found on the following links: $error_links"
}

# remove links
remove_hw_sio_link [get_hw_sio_links {localhost:3121/*/IBERT/Quad_115/MGT_*/TX->localhost:3121/*/IBERT/Quad_115/MGT_*/RX}]

puts "Sleep for 10 seconds...."
exec sleep 10
puts "########################################"
puts "Changing GTX routing to have 4 channels on FMC P2....."
exec $MMC_PATH/scripts/mgtmux.sh -d /dev/ttyUSB3 M2
exec sleep 10

# QSFP2
set xil_newLinks [list]
set xil_newLink [create_hw_sio_link -description {Link 4} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y0/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y0/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 5} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y1/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y1/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 6} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y2/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y2/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLink [create_hw_sio_link -description {Link 7} [lindex [get_hw_sio_txs localhost:3121/*/IBERT/Quad_115/MGT_X0Y3/TX] 0] [lindex [get_hw_sio_rxs localhost:3121/*/IBERT/Quad_115/MGT_X0Y3/RX] 0] ]
lappend xil_newLinks $xil_newLink
set xil_newLinkGroup [create_hw_sio_linkgroup -description {Link Group 1} [get_hw_sio_links $xil_newLinks]]

puts "Sleep for 20 seconds...."
exec sleep 20
set errors 0
set thres 1e-11
set error_links []
refresh_hw_sio [get_hw_sio_links $xil_newLinks]
for {set y 0} {$y<1} {incr y} {
    puts "#####################################"
    puts "FMC2"
    puts "#####################################"
    for {set x 0} {$x<4} {incr x} {
        puts "Link $x status"
        set l [expr $x + 4 * $y]
        set status [get_property STATUS [lindex [get_hw_sio_links] $l]]
        report_property -pattern STATUS [lindex [get_hw_sio_links] $l]
        if {[string match "NO LINK" $status]} {
            incr errors
            lappend error_links "FMC2 Link $x (NO LINK)"
        } else {
            if {[string match "10.000 Gbps" $status]} {
                set ber [get_property RX_BER [lindex [get_hw_sio_links] $l]]
                report_property -pattern RX_BER [lindex [get_hw_sio_links] $l]
                if {[expr $ber > $thres]} {
                    incr errors
                    lappend error_links "FMC2 Link $x (RX_BER > $thres)"
                }
            } else {
                incr errors
                lappend error_links "FMC2 Link $x (Speed not 10.000 Gbps)"
            }
        }
        puts "*************************************"
     }
}

if {$errors > 0} {
    error "Errors found on the following links: $error_links"
}

exec $MMC_PATH/scripts/mgtmux.sh -d /dev/ttyUSB3 M4
exec python3 $MMC_PATH/scripts/load.py -d /dev/ttyUSB3 “r on”
exit
