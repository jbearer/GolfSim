# Take a shot from the origin in the positive X direction, using average PGA
# tour pitching wedge statistics from Trackman:
#
#     Ball speed | Launch angle | Spin rate
#   -------------|--------------|-------------
#       102mph   |     24.2°    |  9304rpm
#
# According to trackman, this shot should have the following trajectory:
#
#       Carry    |     Apex     | Land angle
#   -------------|--------------|-------------
#        136y    |      29y     |     52°
#

round drop 0 0
    # Start from the origin

round swing 0.0454864 0 0.0204417                           0 -0.1551 0
            # 24.2deg launch angle, 102mph exit velocity    # 9304rpm backspin
