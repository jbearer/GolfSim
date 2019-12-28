# Take a shot from the origin in the positive X direction, using average PGA
# tour 7-iron statistics from Trackman:
#
#     Ball speed | Launch angle | Spin rate
#   -------------|--------------|-------------
#       120mph   |     16.3°    |  7097rpm
#
# According to trackman, this shot should have the following trajectory:
#
#       Carry    |     Apex     | Land angle
#   -------------|--------------|-------------
#        172y    |      32y     |     50°
#

round drop 0 0
    # Start from the origin

round swing 0.05867 0 0.0171551                            0 -0.1183 0
            # 16.3deg launch angle, 120mph exit velocity   # 7097rpm backspin
