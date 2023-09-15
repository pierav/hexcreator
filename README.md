# hexcreator

Create memory images for RTL simulation.

Exemple

```sh
# Build app
make

# Create respath from <binfile>
# Memory system is:
#
# 4-4-16:
#            +-------+
#            |  ...  |
#          +-------+ |
#          | ram1  | |
#      ^ +-------+ | |
#      | | ram0  | | | 
#      | |       | |-+  ^ 
#  2048| |       | |   /
# lines| |       |-+  / x16*4*4
#      | |       |   /
#      v +-------+  v
#       <------->
#        8bytes
#
# @D@ means divide by ADDR, DATA, ADDR
#
#  Divide by data: 
#
#              | 128
#           +==+==+
#           | 64  | 64
#        +-----+-----+
#        |     |     |
#        | sub | sub |<==+=== @ [0:N]
#        | mem | mem |   |
#        |     |     |   |
#        |     |<========+
#        +-----+-----+
#           |     |
#           +==+==+
#              | 128
#
#  Divide by addr:  
#             
#              | 128
#        +-----------+
#        |  sub mem  |<==== @ [0:N/2]
#        |           |
#        +-----------+
#        |  sub mem  |<==== @ [N/2:N]
#        |           |
#        +-----------+
#              | 128

./hexcreator test/sample.bin test 4-4-16 @D@ 8 2048 "set cuts(%,%,%) @" > test/cuts.tcl

# Display generated file in 'test' folder
ll test
cut000.hex  cut001.hex ... cut063.hex main.hex cuts.tcl

# We generated here a tcl script from template arg "set cuts(%,%,%) @"
cat /test/cuts.tcl
# set cuts(0,0,0) test/cut000.hex
# set cuts(0,0,1) test/cut001.hex
# set cuts(0,0,2) test/cut002.hex
# set cuts(0,0,3) test/cut003.hex
# set cuts(0,1,0) test/cut004.hex
# set cuts(0,1,1) test/cut005.hex
# ...
# set cuts(3,3,1) test/cut061.hex
# set cuts(3,3,2) test/cut062.hex
# set cuts(3,3,3) test/cut063.hex


# Display TCL variable in generated script
echo "source test/cuts.tcl; parray cuts" | tclsh
# cuts(0,0,0) = test/cut000.hex
# cuts(0,0,1) = test/cut001.hex
# cuts(0,0,2) = test/cut002.hex
# cuts(0,0,3) = test/cut003.hex
# cuts(0,1,0) = test/cut004.hex
# ...
# cuts(3,3,1) = test/cut061.hex
# cuts(3,3,2) = test/cut062.hex
# cuts(3,3,3) = test/cut063.hex

```

