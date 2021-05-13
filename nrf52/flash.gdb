#set trace-commands on
source flash_config.gdb

set gnutarget elf32-littlearm
if $USE_EXTERNAL_DEBUGGER
  monitor tpwr enable
else
  set tdesc filename gdb-tdesc-cortex-m4f.xml
end
monitor swdp_scan

if $FORCE_ERASE_ALL
  set non-stop on
python
def ignore_error(cmd):
  try:
    gdb.execute(cmd)
  except:
    gdb.execute("echo ignoring error\\n")
    pass
ignore_error("attach 1")
end
  monitor erase_mass
  monitor swdp_scan
  set non-stop off
end
set mem inaccessible-by-default off
set debug arm
attach 1

if $LOAD_SOFTDEVICE
  load nordicsdk/softdevice/s132/hex/s132_nrf52_5.0.0_softdevice.hex
end

load builds/nsec19_nrf52_ctf.out
file builds/nsec19_nrf52_ctf.out

if $LOCK_AFTER_FLASH
  monitor enable_approtect
end

define hook-quit
  set confirm off
end

if $QUIT_AFTER_FLASH
  quit
else
  run
end
