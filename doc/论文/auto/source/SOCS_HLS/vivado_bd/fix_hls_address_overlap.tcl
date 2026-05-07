# ============================================================================
# Vivado Block Design Address Fix Script for calc_socs_hls IP
# ============================================================================
# Purpose: Fix address overlap issue when HLS IP with multiple m_axi interfaces
#          is imported into Vivado Block Design
# 
# Usage: Run in Vivado Tcl Console after importing HLS IP:
#        source fix_hls_address_overlap.tcl
#
# Author: FPGA-Litho Project
# Date: 2026-04-14
# ============================================================================

# Configuration parameters
set IP_NAME        "calc_socs_hls_1"
set AXI_MASTER     "jtag_axi_0"  ;# Or your processor's AXI master (e.g., microblaze_0/M_AXI_DP)

# Address allocation table (based on HLS depth parameters)
# Format: {interface_name base_addr size_in_bytes description}
set ADDRESS_MAP {
    {m_axi_gmem0  0x40000000  1048576  "mskf_r - Mask spectrum real (512×512 float = 1 MB)"}
    {m_axi_gmem1  0x40100000  1048576  "mskf_i - Mask spectrum imaginary (512×512 float = 1 MB)"}
    {m_axi_gmem2  0x40200000  64      "scales - Eigenvalues (10 float = 40B, reserve 64B)"}
    {m_axi_gmem3  0x40200100  4096    "krn_r - Kernels real (10×9×9 float = 810B, reserve 4KB)"}
    {m_axi_gmem4  0x40201000  4096    "krn_i - Kernels imaginary (10×9×9 float = 810B, reserve 4KB)"}
    {m_axi_gmem5  0x40202000  2048    "output - Result (17×17 float = 289B, reserve 2KB)"}
}

# ============================================================================
# Main Procedure
# ============================================================================
proc fix_hls_ip_address_overlap {ip_name axi_master address_map} {
    puts "============================================================"
    puts "Fixing HLS IP Address Overlap for: $ip_name"
    puts "============================================================"
    
    # Check if IP exists in Block Design
    if {[catch {get_bd_cells $ip_name} err]} {
        puts "ERROR: IP '$ip_name' not found in Block Design!"
        puts "Please import HLS IP first and run this script again."
        return -code error $err
    }
    
    # Check if AXI master exists
    if {[catch {get_bd_cells $axi_master} err]} {
        puts "WARNING: AXI master '$axi_master' not found."
        puts "Trying to find alternative master..."
        
        # Try common alternatives
        set alternatives {"jtag_axi_0" "microblaze_0" "ps7_0" "zynq_ultra_ps_e_0"}
        foreach alt $alternatives {
            if {[catch {get_bd_cells $alt}]==0} {
                set axi_master $alt
                puts "Found alternative: $axi_master"
                break
            }
        }
    }
    
    # Get master address segment path
    set master_seg "${axi_master}/Data"
    if {[catch {get_bd_addr_segs $master_seg} err]} {
        # Try other common segment paths
        set master_seg "${axi_master}/M_AXI_DP/Data"
        if {[catch {get_bd_addr_segs $master_seg} err]} {
            puts "ERROR: Cannot find address segment for '$axi_master'"
            return -code error $err
        }
    }
    
    puts "Using master address segment: $master_seg"
    puts ""
    
    # Process each AXI-Master interface
    foreach entry $address_map {
        set interface   [lindex $entry 0]
        set base_addr   [lindex $entry 1]
        set size        [lindex $entry 2]
        set description [lindex $entry 3]
        
        # Calculate end address
        set end_addr [expr $base_addr + $size - 1]
        
        puts "Assigning: $interface"
        puts "  Base:   0x[format %08X $base_addr]"
        puts "  End:    0x[format %08X $end_addr]"
        puts "  Size:   $size bytes"
        puts "  Desc:   $description"
        
        # Get interface address segment
        set ip_seg "${ip_name}/${interface}"
        if {[catch {get_bd_addr_segs $ip_seg} seg_err]} {
            puts "WARNING: Interface '$interface' not found, skipping..."
            puts ""
            continue
        }
        
        # Assign address
        if {[catch {
            assign_bd_address [get_bd_addr_segs $ip_seg] \
                [get_bd_addr_segs $master_seg] \
                $base_addr $end_addr
        } assign_err]} {
            puts "WARNING: Failed to assign address for '$interface'"
            puts "  Error: $assign_err"
            puts ""
            continue
        }
        
        puts "  SUCCESS: Address assigned!"
        puts ""
    }
    
    # Validate Block Design
    puts "Validating Block Design..."
    if {[catch {validate_bd_design} validate_err]} {
        puts "ERROR: Block Design validation failed!"
        puts "  $validate_err"
        return -code error $validate_err
    }
    
    puts "============================================================"
    puts "SUCCESS: All addresses assigned without overlap!"
    puts "============================================================"
    
    # Print final address summary
    puts ""
    puts "Address Allocation Summary:"
    puts "------------------------------------------------------------"
    foreach entry $address_map {
        set interface   [lindex $entry 0]
        set base_addr   [lindex $entry 1]
        set size        [lindex $entry 2]
        set end_addr    [expr $base_addr + $size - 1]
        puts "[format %-12s $interface]: 0x[format %08X $base_addr] - 0x[format %08X $end_addr] ([format %6d $size] bytes)"
    }
    puts "------------------------------------------------------------"
    
    # Save Block Design
    puts ""
    puts "Saving Block Design..."
    save_bd_design
    
    puts "Done! You can now export hardware and proceed to SDK/Vitis."
}

# ============================================================================
# Execute the fix
# ============================================================================
fix_hls_ip_address_overlap $IP_NAME $AXI_MASTER $ADDRESS_MAP

# ============================================================================
# Optional: Generate report of current address allocation
# ============================================================================
puts ""
puts "Generating Address Summary Report..."
report_address_summary