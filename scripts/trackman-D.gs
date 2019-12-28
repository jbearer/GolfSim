# Take a shot from the origin in the positive X direction, using average PGA
# tour driver statistics from Trackman:
#
#     Ball speed | Launch angle | Spin rate
#   -------------|--------------|-------------
#       167mph   |     10.9°    |  2686rpm
#
# According to trackman, this shot should have the following trajectory:
#
#       Carry    |     Apex     | Land angle
#   -------------|--------------|-------------
#        275y    |     32y      |     38°
#

round drop 0 0
    # Start from the origin

round swing 0.0801607 0 0.015471                            0 -0.04352 0
            # 10.9deg launch angle, 167mph exit velocity    # 2611rpm backspin
