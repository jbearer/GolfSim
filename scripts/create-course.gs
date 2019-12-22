###############################################################
# Routing
#
#                   Hole  Tee       Shot 1    Shot 2    Shot 3
terrain define-hole 1     5 5       23 10
terrain define-hole 2     30 10     30 35     25 50     25 60
terrain define-hole 3     23 63     5 45      5 30

# Hole 1 textures
terrain set      5 5            tee
terrain bulk-set 21 10  21 11   sand
terrain bulk-set 22 8   24 8    sand
terrain set      21 9           fairway
terrain bulk-set 19 9   20 11   fairway
terrain bulk-set 22 9   24 11   green

# Hole 1 elevations
terrain raise-face      5 5             1
terrain bulk-raise-face 22 9    24 11   1

# Hole 2 textures
terrain set      30 10          tee
terrain bulk-set 29 30  31 45   fairway
terrain bulk-set 24 48  26 48   sand
terrain bulk-set 24 49  26 58   fairway
terrain bulk-set 24 59  26 61   green

# Hole 2 elevations
terrain bulk-raise-face 24 49   26 50   1
terrain bulk-raise-face 23 59   27 63   1

# Hole 3 textures
terrain set      23 63          tee
terrain bulk-set 5 30   7 37    fairway
terrain bulk-set 4 35   6 47    fairway
terrain bulk-set 5 46   7 48    fairway
terrain bulk-set 6 44   6 45    sand
terrain set      5 31           sand
terrain bulk-set 6 30   6 31    green
terrain set      5 30           green

# Hole 3 elevations
terrain bulk-raise-face 5 43    6 43    1
terrain raise-face      5 30            1
terrain raise-face      4 35            1

show routing
terrain info routing
