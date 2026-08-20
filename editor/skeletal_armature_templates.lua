--[[
-------------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|------------------------------------------------------------------------------------------------------------------------|

   Built-in Armature Templates for the canonical Skeletal Animation Editor worktree.
]]--

local ARMATURE_STANDARD_SKELETON_65 = {
    label = 'Standard Skeleton (65)',
    referenceAABB = { minX=-0.953590, minY=-0.000157, minZ=-0.163369, maxX=0.952638, maxY=1.913079, maxZ=0.239427 },
    bones = {
        { name = 'mixamorig:Hips', parent = nil, x=0.001954, y=1.018964, z=-0.027472, radius=0.017810, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.118735 },
        { name = 'mixamorig:Spine', parent = 'mixamorig:Hips', x=0.001954, y=1.126944, z=-0.036337, radius=0.018960, rotX=-4.693449, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.126400 },
        { name = 'mixamorig:LeftUpLeg', parent = 'mixamorig:Hips', x=0.110291, y=0.958871, z=-0.030830, radius=0.062366, rotX=-3.047297, rotY=0.360019, rotZ=-173.265015, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.415772 },
        { name = 'mixamorig:RightUpLeg', parent = 'mixamorig:Hips', x=-0.106384, y=0.958871, z=-0.030655, radius=0.062368, rotX=-3.077373, rotY=-0.363550, rotZ=173.265381, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.415784 },
        { name = 'mixamorig:Spine1', parent = 'mixamorig:Spine', x=0.001954, y=1.252921, z=-0.046680, radius=0.021669, rotX=-4.693448, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144458 },
        { name = 'mixamorig:LeftLeg', parent = 'mixamorig:LeftUpLeg', x=0.159121, y=0.546568, z=-0.052932, radius=0.060870, rotX=-2.687390, rotY=0.407074, rotZ=-172.385559, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405798 },
        { name = 'mixamorig:RightLeg', parent = 'mixamorig:RightUpLeg', x=-0.155213, y=0.546568, z=-0.052976, radius=0.060875, rotX=-2.792445, rotY=-0.410901, rotZ=172.386490, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405834 },
        { name = 'mixamorig:Spine2', parent = 'mixamorig:Spine1', x=0.001954, y=1.396894, z=-0.058500, radius=0.024393, rotX=-4.693454, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.162619 },
        { name = 'mixamorig:LeftFoot', parent = 'mixamorig:LeftLeg', x=0.212967, y=0.144808, z=-0.071958, radius=0.032511, rotX=51.206329, rotY=-9.083311, rotZ=-171.936569, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216739 },
        { name = 'mixamorig:RightFoot', parent = 'mixamorig:RightLeg', x=-0.209059, y=0.144808, z=-0.072746, radius=0.032943, rotX=51.806446, rotY=8.882802, rotZ=171.929749, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.219620 },
        { name = 'mixamorig:Neck', parent = 'mixamorig:Spine2', x=0.001954, y=1.558864, z=-0.071798, radius=0.008050, rotX=0.000000, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.053668 },
        { name = 'mixamorig:LeftShoulder', parent = 'mixamorig:Spine2', x=0.075299, y=1.541482, z=-0.071794, radius=0.022655, rotX=0.029381, rotY=85.177055, rotZ=-103.269150, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151035 },
        { name = 'mixamorig:RightShoulder', parent = 'mixamorig:Spine2', x=-0.071392, y=1.541482, z=-0.071801, radius=0.022655, rotX=-0.029374, rotY=-85.175842, rotZ=103.327698, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151035 },
        { name = 'mixamorig:LeftToeBase', parent = 'mixamorig:LeftFoot', x=0.258419, y=0.014100, z=0.094852, radius=0.013121, rotX=91.257622, rotY=-13.754485, rotZ=-177.986542, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.087472 },
        { name = 'mixamorig:RightToeBase', parent = 'mixamorig:RightFoot', x=-0.254512, y=0.014099, z=0.097788, radius=0.013472, rotX=91.201202, rotY=13.391500, rotZ=177.886215, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.089811 },
        { name = 'mixamorig:Head', parent = 'mixamorig:Neck', x=0.001954, y=1.609482, z=-0.053962, radius=0.046270, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.308468 },
        { name = 'mixamorig:LeftArm', parent = 'mixamorig:LeftShoulder', x=0.222285, y=1.506740, z=-0.071788, radius=0.040572, rotX=9.747616, rotY=84.915649, rotZ=-84.800674, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.270482 },
        { name = 'mixamorig:RightArm', parent = 'mixamorig:RightShoulder', x=-0.218377, y=1.506740, z=-0.071807, radius=0.040571, rotX=7.950571, rotY=-84.964287, rotZ=86.590340, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.270472 },
        { name = 'mixamorig:LeftToe_End', parent = 'mixamorig:LeftToeBase', x=0.279131, y=0.016750, z=0.179795, radius=0.013121, rotX=91.257614, rotY=-13.754465, rotZ=-177.986481, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.087472 },
        { name = 'mixamorig:RightToe_End', parent = 'mixamorig:RightToeBase', x=-0.275224, y=0.016748, z=0.185138, radius=0.013472, rotX=91.201180, rotY=13.391499, rotZ=177.886154, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.089811 },
        { name = 'mixamorig:HeadTop_End', parent = 'mixamorig:Head', x=0.001954, y=1.900418, z=0.048550, radius=0.046270, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.308468 },
        { name = 'mixamorig:LeftForeArm', parent = 'mixamorig:LeftArm', x=0.491899, y=1.485470, z=-0.067729, radius=0.033055, rotX=1.181309, rotY=84.945183, rotZ=-86.996162, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.220369 },
        { name = 'mixamorig:RightForeArm', parent = 'mixamorig:RightArm', x=-0.487991, y=1.485471, z=-0.068524, radius=0.033057, rotX=-5.899307, rotY=-85.012611, rotZ=94.050064, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.220377 },
        { name = 'mixamorig:LeftHand', parent = 'mixamorig:LeftForeArm', x=0.712155, y=1.492497, z=-0.067329, radius=0.020135, rotX=53.151276, rotY=82.267815, rotZ=-38.823112, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.134235 },
        { name = 'mixamorig:RightHand', parent = 'mixamorig:RightForeArm', x=-0.708247, y=1.492497, z=-0.070493, radius=0.023043, rotX=62.126709, rotY=-80.576805, rotZ=29.662706, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.153618 },
        { name = 'mixamorig:LeftHandIndex1', parent = 'mixamorig:LeftHand', x=0.845549, y=1.488484, z=-0.052877, radius=0.004506, rotX=-75.147537, rotY=75.644257, rotZ=-163.785980, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030040 },
        { name = 'mixamorig:RightHandIndex1', parent = 'mixamorig:RightHand', x=-0.860198, y=1.488607, z=-0.048260, radius=0.003849, rotX=7.909595, rotY=-85.458687, rotZ=77.243103, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.025658 },
        { name = 'mixamorig:LeftHandIndex2', parent = 'mixamorig:LeftHandIndex1', x=0.874710, y=1.488945, z=-0.060076, radius=0.006490, rotX=-75.147240, rotY=75.644264, rotZ=-163.786072, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.043267 },
        { name = 'mixamorig:RightHandIndex2', parent = 'mixamorig:RightHandIndex1', x=-0.885762, y=1.490785, z=-0.047980, radius=0.005510, rotX=7.908145, rotY=-85.458572, rotZ=77.243698, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036731 },
        { name = 'mixamorig:LeftHandIndex3', parent = 'mixamorig:LeftHandIndex2', x=0.916940, y=1.486856, z=-0.050896, radius=0.004576, rotX=-75.147484, rotY=75.644241, rotZ=-163.786026, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030509 },
        { name = 'mixamorig:RightHandIndex3', parent = 'mixamorig:RightHandIndex2', x=-0.920641, y=1.483321, z=-0.039209, radius=0.003606, rotX=7.908519, rotY=-85.458641, rotZ=77.243500, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.024040 },
        { name = 'mixamorig:LeftHandIndex4', parent = 'mixamorig:LeftHandIndex3', x=0.945577, y=1.482247, z=-0.041434, radius=0.004576, rotX=-75.147255, rotY=75.644241, rotZ=-163.786026, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030510 },
        { name = 'mixamorig:RightHandIndex4', parent = 'mixamorig:RightHandIndex3', x=-0.944469, y=1.481889, z=-0.036367, radius=0.003606, rotX=7.908629, rotY=-85.458641, rotZ=77.243385, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.024040 },
    },
}
local ARMATURE_3_CHAIN_FINGERS = {
    label = '3 Chain Fingers (49)',
    referenceAABB = { minX=-0.740790, minY=-0.000114, minZ=-0.151165, maxX=0.748511, maxY=1.822469, maxZ=0.208525 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.926227, z=-0.028060, radius=0.016094, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.107293 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.039920, z=-0.026879, radius=0.019897, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.132649 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082740, y=0.863078, z=-0.029367, radius=0.056145, rotX=-2.653563, rotY=-0.070910, rotZ=178.470764, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374301 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082740, y=0.863078, z=-0.028519, radius=0.056171, rotX=-3.163926, rotY=0.084448, rotZ=-178.472153, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374471 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.172562, z=-0.025500, radius=0.022740, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151599 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.072740, y=0.489312, z=-0.046696, radius=0.058006, rotX=-0.079035, rotY=0.081460, rotZ=-178.241730, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.386709 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.072740, y=0.489312, z=-0.049187, radius=0.058006, rotX=0.019028, rotY=-0.097177, rotZ=178.241592, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.386709 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.324153, z=-0.023925, radius=0.024670, rotX=0.595406, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.164464 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084606, y=0.102785, z=-0.047229, radius=0.023382, rotX=50.269875, rotY=-15.468593, rotZ=-178.219757, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.155881 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084606, y=0.102785, z=-0.049059, radius=0.022571, rotX=48.543678, rotY=16.496124, rotZ=178.247269, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150471 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.494692, z=-0.022152, radius=0.011283, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.075221 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.065546, y=1.471656, z=-0.022637, radius=0.021686, rotX=-143.193878, rotY=89.357948, rotZ=110.597511, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144574 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.065546, y=1.471656, z=-0.021668, radius=0.021686, rotX=153.063095, rotY=-89.151520, rotZ=-46.858749, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.144574 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.119660, y=0.004191, z=0.068311, radius=0.009166, rotX=90.589684, rotY=-16.677780, rotZ=-179.745148, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061109 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.119660, y=0.004192, z=0.059072, radius=0.008818, rotX=90.473701, rotY=17.362391, rotZ=179.277725, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058784 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.569418, z=-0.013541, radius=0.037451, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.249673 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.204372, y=1.431306, z=-0.023607, radius=0.026980, rotX=150.208664, rotY=89.433632, rotZ=48.879364, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.179865 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.204372, y=1.431306, z=-0.020698, radius=0.026981, rotX=140.853973, rotY=-89.084854, rotZ=-39.527004, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.179872 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.137194, y=0.004898, z=0.126847, radius=0.009166, rotX=90.589691, rotY=-16.677790, rotZ=-179.745163, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061109 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.137194, y=0.004899, z=0.115175, radius=0.008818, rotX=90.473709, rotY=17.362394, rotZ=179.277710, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058784 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817449, z=0.015044, radius=0.037451, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.249673 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.380729, y=1.395968, z=-0.022724, radius=0.036149, rotX=-107.850189, rotY=84.812241, rotZ=163.251862, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.240990 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.380729, y=1.395968, z=-0.018885, radius=0.036310, rotX=-107.620689, rotY=-82.328033, rotZ=-163.400940, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.242063 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.620775, y=1.400874, z=-0.043465, radius=0.010102, rotX=23.350143, rotY=-64.426994, rotZ=-78.428665, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.067345 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.620775, y=1.400874, z=-0.049684, radius=0.009249, rotX=16.918432, rotY=0.721628, rotZ=88.736382, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061660 },
        { name = "mixamorig:LeftHandIndex1", parent = "mixamorig:LeftHand", x=0.676519, y=1.436865, z=-0.031943, radius=0.003127, rotX=-155.008316, rotY=46.454838, rotZ=112.320648, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.020848 },
        { name = "mixamorig:RightHandIndex1", parent = "mixamorig:RightHand", x=-0.679747, y=1.402401, z=-0.031742, radius=0.002619, rotX=-166.525299, rotY=14.309860, rotZ=-95.931923, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.017460 },
        { name = "mixamorig:LeftHandIndex2", parent = "mixamorig:LeftHandIndex1", x=0.696423, y=1.438135, z=-0.038011, radius=0.003453, rotX=-155.008545, rotY=46.454731, rotZ=112.320518, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.023023 },
        { name = "mixamorig:RightHandIndex2", parent = "mixamorig:RightHandIndex1", x=-0.696531, y=1.405156, z=-0.035684, radius=0.003295, rotX=-166.525467, rotY=14.310186, rotZ=-95.932716, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.021967 },
        { name = "mixamorig:LeftHandIndex3", parent = "mixamorig:LeftHandIndex2", x=0.717848, y=1.434085, z=-0.045402, radius=0.001695, rotX=-155.008423, rotY=46.454708, rotZ=112.320976, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.011303 },
        { name = "mixamorig:RightHandIndex3", parent = "mixamorig:RightHandIndex2", x=-0.718471, y=1.404729, z=-0.034694, radius=0.001817, rotX=-166.525497, rotY=14.310227, rotZ=-95.932892, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.012116 },
        { name = "mixamorig:LeftHandIndex4", parent = "mixamorig:LeftHandIndex3", x=0.728734, y=1.434607, z=-0.048399, radius=0.001695, rotX=-155.008545, rotY=46.454552, rotZ=112.321465, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.011303 },
        { name = "mixamorig:RightHandIndex4", parent = "mixamorig:RightHandIndex3", x=-0.730570, y=1.404833, z=-0.035324, radius=0.001817, rotX=-166.525482, rotY=14.310113, rotZ=-95.932343, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.012116 },
    },
}

local ARMATURE_2_CHAIN_FINGERS = {
    label = '2 Chain Fingers (41)',
    referenceAABB = { minX=-0.741990, minY=-0.000168, minZ=-0.151150, maxX=0.748067, maxY=1.821747, maxZ=0.208540 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.927429, z=-0.028792, radius=0.016085, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.107235 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.041066, z=-0.027515, radius=0.019888, rotX=0.643814, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.132585 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082532, y=0.864099, z=-0.029326, radius=0.059681, rotX=-3.342882, rotY=-0.073730, rotZ=178.736511, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.397875 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082532, y=0.864099, z=-0.028602, radius=0.059699, rotX=-3.620366, rotY=0.079793, rotZ=-178.737259, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.397992 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.173643, z=-0.026025, radius=0.022729, rotX=0.643816, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.151526 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.073743, y=0.466998, z=-0.052527, radius=0.054645, rotX=0.851641, rotY=0.097370, rotZ=-178.333740, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.364302 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.073743, y=0.466998, z=-0.053734, radius=0.054644, rotX=0.735102, rotY=-0.105518, rotZ=178.333832, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.364292 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.325159, z=-0.024323, radius=0.024789, rotX=0.643816, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165261 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084326, y=0.102890, z=-0.047112, radius=0.023237, rotX=50.019455, rotY=-16.240631, rotZ=-178.583298, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.154912 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084326, y=0.102890, z=-0.049060, radius=0.022601, rotX=48.646500, rotY=17.048336, rotZ=178.560593, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150670 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.495614, z=-0.022407, radius=0.010582, rotX=-0.000003, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.070544 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.066726, y=1.473488, z=-0.022656, radius=0.021072, rotX=-161.794159, rotY=89.351067, rotZ=91.020599, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.140482 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.066726, y=1.473488, z=-0.022159, radius=0.021072, rotX=164.733261, rotY=-89.229683, rotZ=-57.550373, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.140482 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.119975, y=0.004206, z=0.066855, radius=0.009153, rotX=90.030121, rotY=-17.883352, rotZ=179.345535, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061020 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.119975, y=0.004206, z=0.059070, radius=0.008873, rotX=89.909355, rotY=18.468466, rotZ=-179.729736, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.059150 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.565681, z=-0.014211, radius=0.038072, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.253814 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.200936, y=1.431984, z=-0.023153, radius=0.030288, rotX=-133.360474, rotY=88.972885, rotZ=125.933090, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.201919 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.200936, y=1.431984, z=-0.021661, radius=0.030286, rotX=161.655945, rotY=-89.240746, rotZ=-60.955521, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.201904 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.138712, y=0.004024, z=0.124926, radius=0.009153, rotX=90.030121, rotY=-17.883347, rotZ=179.345535, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061020 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.138712, y=0.004024, z=0.115174, radius=0.008872, rotX=89.909355, rotY=18.468479, rotZ=-179.729752, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.059150 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817776, z=0.015277, radius=0.038072, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.253814 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.399326, y=1.394491, z=-0.025785, radius=0.026545, rotX=-110.919655, rotY=86.316376, rotZ=158.264435, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.176967 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.399326, y=1.394491, z=-0.020819, radius=0.026701, rotX=-105.398102, rotY=-82.650970, rotZ=-163.704132, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.178006 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.575957, y=1.392098, z=-0.036406, radius=0.003586, rotX=-91.620926, rotY=-84.495583, rotZ=50.624012, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.023906 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.575957, y=1.392097, z=-0.042771, radius=0.004617, rotX=-85.864952, rotY=-78.748955, rotZ=-169.145660, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.030777 },
        { name = "mixamorig:LeftHandIndex1", parent = "mixamorig:LeftHand", x=0.591570, y=1.410055, z=-0.038698, radius=0.006338, rotX=-40.247967, rotY=84.203163, rotZ=-112.600563, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.042254 },
        { name = "mixamorig:RightHandIndex1", parent = "mixamorig:RightHand", x=-0.605108, y=1.384248, z=-0.048761, radius=0.005585, rotX=15.452384, rotY=57.404198, rotZ=86.994911, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.037235 },
        { name = "mixamorig:LeftHandIndex2", parent = "mixamorig:LeftHandIndex1", x=0.631781, y=1.422736, z=-0.041455, radius=0.009662, rotX=-40.247978, rotY=84.203140, rotZ=-112.600677, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.064414 },
        { name = "mixamorig:RightHandIndex2", parent = "mixamorig:RightHandIndex1", x=-0.640509, y=1.394476, z=-0.043416, radius=0.008474, rotX=15.452312, rotY=57.404232, rotZ=86.994835, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.056496 },
        { name = "mixamorig:LeftHandIndex3", parent = "mixamorig:LeftHandIndex2", x=0.694163, y=1.438223, z=-0.037243, radius=0.005476, rotX=-40.247971, rotY=84.203194, rotZ=-112.600685, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036505 },
        { name = "mixamorig:RightHandIndex3", parent = "mixamorig:RightHandIndex2", x=-0.695453, y=1.405216, z=-0.035825, radius=0.005269, rotX=15.452374, rotY=57.404217, rotZ=86.994804, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.035124 },
        { name = "mixamorig:LeftHandIndex4", parent = "mixamorig:LeftHandIndex3", x=0.728734, y=1.434608, z=-0.048399, radius=0.005476, rotX=-40.247986, rotY=84.203194, rotZ=-112.600700, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.036505 },
        { name = "mixamorig:RightHandIndex4", parent = "mixamorig:RightHandIndex3", x=-0.730571, y=1.404832, z=-0.035324, radius=0.005269, rotX=15.452349, rotY=57.404179, rotZ=86.994995, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.035124 },
    },
}
local ARMATURE_NO_FINGERS_25 = {
    label = 'No Fingers (25)',
    referenceAABB = { minX=-0.733316, minY=-0.000161, minZ=-0.151244, maxX=0.736469, maxY=1.822023, maxZ=0.208446 },
    bones = {
        { name = "mixamorig:Hips", parent = nil, x=0.000000, y=0.936117, z=-0.029040, radius=0.015899, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.105995 },
        { name = "mixamorig:Spine", parent = "mixamorig:Hips", x=0.000000, y=1.047633, z=-0.027799, radius=0.019516, rotX=0.637164, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.130110 },
        { name = "mixamorig:LeftUpLeg", parent = "mixamorig:Hips", x=0.082588, y=0.874185, z=-0.029391, radius=0.060107, rotX=-3.232387, rotY=-0.083342, rotZ=178.523285, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.400712 },
        { name = "mixamorig:RightUpLeg", parent = "mixamorig:Hips", x=-0.082588, y=0.874185, z=-0.028313, radius=0.060107, rotX=-3.232189, rotY=0.083352, rotZ=-178.523285, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.400712 },
        { name = "mixamorig:Spine1", parent = "mixamorig:Spine", x=0.000000, y=1.177734, z=-0.026352, radius=0.022304, rotX=0.637164, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.148697 },
        { name = "mixamorig:LeftLeg", parent = "mixamorig:LeftUpLeg", x=0.072245, y=0.474243, z=-0.051986, radius=0.055767, rotX=0.733447, rotY=0.107789, rotZ=-178.092239, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.371778 },
        { name = "mixamorig:RightLeg", parent = "mixamorig:RightUpLeg", x=-0.072245, y=0.474243, z=-0.050906, radius=0.055763, rotX=0.325850, rotY=-0.107725, rotZ=178.093002, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.371753 },
        { name = "mixamorig:Spine2", parent = "mixamorig:Spine1", x=0.000000, y=1.326421, z=-0.024699, radius=0.024506, rotX=0.637165, rotY=0.000000, rotZ=0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.163376 },
        { name = "mixamorig:LeftFoot", parent = "mixamorig:LeftLeg", x=0.084611, y=0.102702, z=-0.047227, radius=0.023352, rotX=50.271675, rotY=-15.199527, rotZ=-178.292892, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.155678 },
        { name = "mixamorig:RightFoot", parent = "mixamorig:RightLeg", x=-0.084611, y=0.102702, z=-0.048792, radius=0.022597, rotX=48.633934, rotY=16.022581, rotZ=178.187195, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.150649 },
        { name = "mixamorig:Neck", parent = "mixamorig:Spine2", x=0.000000, y=1.493695, z=-0.022839, radius=0.008616, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.057438 },
        { name = "mixamorig:LeftShoulder", parent = "mixamorig:Spine2", x=0.068881, y=1.472398, z=-0.023094, radius=0.021457, rotX=-161.087799, rotY=89.368996, rotZ=90.200729, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.143049 },
        { name = "mixamorig:RightShoulder", parent = "mixamorig:Spine2", x=-0.068881, y=1.472398, z=-0.022583, radius=0.021457, rotX=164.397141, rotY=-89.239380, rotZ=-55.688076, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.143049 },
        { name = "mixamorig:LeftToeBase", parent = "mixamorig:LeftFoot", x=0.118952, y=0.004180, z=0.068314, radius=0.009197, rotX=90.956543, rotY=-17.296185, rotZ=179.044830, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061317 },
        { name = "mixamorig:RightToeBase", parent = "mixamorig:RightFoot", x=-0.118952, y=0.004180, z=0.059879, radius=0.008735, rotX=90.978867, rotY=18.245518, rotZ=-179.134781, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058232 },
        { name = "mixamorig:Head", parent = "mixamorig:Neck", x=0.000000, y=1.550730, z=-0.016052, radius=0.040315, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.268765 },
        { name = "mixamorig:LeftArm", parent = "mixamorig:LeftShoulder", x=0.204369, y=1.426510, z=-0.023605, radius=0.032483, rotX=-145.014679, rotY=89.161446, rotZ=115.907913, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216552 },
        { name = "mixamorig:RightArm", parent = "mixamorig:RightShoulder", x=-0.204369, y=1.426510, z=-0.022073, radius=0.032482, rotX=173.098831, rotY=-89.266853, rotZ=-74.024925, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.216545 },
        { name = "mixamorig:LeftToe_End", parent = "mixamorig:LeftToeBase", x=0.137194, y=0.004900, z=0.126850, radius=0.009197, rotX=90.956543, rotY=-17.296190, rotZ=179.044830, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.061317 },
        { name = "mixamorig:RightToe_End", parent = "mixamorig:RightToeBase", x=-0.137194, y=0.004899, z=0.115175, radius=0.008735, rotX=90.978851, rotY=18.245510, rotZ=-179.134796, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.058232 },
        { name = "mixamorig:HeadTop_End", parent = "mixamorig:Head", x=0.000000, y=1.817613, z=0.015704, radius=0.040315, rotX=-0.000004, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.268765 },
        { name = "mixamorig:LeftForeArm", parent = "mixamorig:LeftArm", x=0.418203, y=1.392357, z=-0.025422, radius=0.028807, rotX=-108.689583, rotY=84.928093, rotZ=163.154465, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.192045 },
        { name = "mixamorig:RightForeArm", parent = "mixamorig:RightArm", x=-0.418203, y=1.392357, z=-0.021740, radius=0.029016, rotX=-105.569199, rotY=-81.288918, rotZ=-166.170456, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.193440 },
        { name = "mixamorig:LeftHand", parent = "mixamorig:LeftForeArm", x=0.609467, y=1.398743, z=-0.041504, radius=0.028807, rotX=-108.689728, rotY=84.928131, rotZ=163.154434, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.192045 },
        { name = "mixamorig:RightHand", parent = "mixamorig:RightForeArm", x=-0.609467, y=1.398744, z=-0.049962, radius=0.029016, rotX=-105.569382, rotY=-81.288940, rotZ=-166.170380, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.193440 },
    },
}

local ARMATURE_NO_FINGERS_23 = {
    label = 'No Fingers (23)',
    referenceAABB = { minX=-0.971253, minY=-0.000000, minZ=-0.829249, maxX=0.971253, maxY=2.136544, maxZ=0.566499 },
    bones = {
        { name = "root", parent = nil, x=0.000000, y=0.000000, z=0.000000, radius=0.060850, rotX=-0.000007, rotY=0.000014, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.405663 },
        { name = "hips", parent = "root", x=0.000000, y=0.405663, z=-0.000000, radius=0.030123, rotX=-0.000007, rotY=-0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.200821 },
        { name = "spine", parent = "hips", x=0.000000, y=0.597641, z=-0.000000, radius=0.056248, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.374988 },
        { name = "upperleg.l", parent = "hips", x=0.170945, y=0.519251, z=-0.000000, radius=0.034062, rotX=178.008987, rotY=0.000009, rotZ=-0.000008, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.227077 },
        { name = "upperleg.r", parent = "hips", x=-0.170945, y=0.519251, z=-0.000000, radius=0.034062, rotX=178.008987, rotY=-0.000009, rotZ=0.000008, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.227077 },
        { name = "chest", parent = "spine", x=0.000000, y=0.972629, z=-0.000000, radius=0.038527, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.256849 },
        { name = "lowerleg.l", parent = "upperleg.l", x=0.170945, y=0.292310, z=0.007889, radius=0.022416, rotX=-169.795273, rotY=-0.000064, rotZ=-0.000006, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.149437 },
        { name = "lowerleg.r", parent = "upperleg.r", x=-0.170945, y=0.292310, z=0.007889, radius=0.022416, rotX=-169.795273, rotY=0.000064, rotZ=0.000006, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.149437 },
        { name = "upperarm.l", parent = "chest", x=0.212007, y=1.106761, z=-0.000000, radius=0.036285, rotX=-90.000000, rotY=-86.715446, rotZ=-0.000001, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.241897 },
        { name = "upperarm.r", parent = "chest", x=-0.212007, y=1.106761, z=-0.000000, radius=0.036285, rotX=-90.000000, rotY=86.715446, rotZ=0.000001, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.241897 },
        { name = "head", parent = "chest", x=0.000000, y=1.241426, z=-0.000000, radius=0.038527, rotX=-0.000007, rotY=0.000000, rotZ=-0.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.256849 },
        { name = "foot.l", parent = "lowerleg.l", x=0.170945, y=0.145237, z=-0.018586, radius=0.024848, rotX=136.043976, rotY=-0.000025, rotZ=0.000010, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "foot.r", parent = "lowerleg.r", x=-0.170945, y=0.145237, z=-0.018586, radius=0.024848, rotX=136.043976, rotY=0.000025, rotZ=-0.000010, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "lowerarm.l", parent = "upperarm.l", x=0.453507, y=1.106761, z=-0.013860, radius=0.039007, rotX=90.000313, rotY=-86.944817, rotZ=179.999695, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.260044 },
        { name = "lowerarm.r", parent = "upperarm.r", x=-0.453507, y=1.106761, z=-0.013860, radius=0.039007, rotX=90.000313, rotY=86.944817, rotZ=-179.999695, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.260044 },
        { name = "toes.l", parent = "foot.l", x=0.170945, y=0.025990, z=0.096393, radius=0.024848, rotX=90.000000, rotY=-0.000010, rotZ=179.999969, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "toes.r", parent = "foot.r", x=-0.170945, y=0.025990, z=0.096393, radius=0.024848, rotX=90.000000, rotY=0.000010, rotZ=-179.999969, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.165650 },
        { name = "wrist.l", parent = "lowerarm.l", x=0.713182, y=1.106761, z=-0.000000, radius=0.011074, rotX=0.000000, rotY=-90.000000, rotZ=-90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.073826 },
        { name = "wrist.r", parent = "lowerarm.r", x=-0.713182, y=1.106761, z=-0.000000, radius=0.011074, rotX=0.000000, rotY=90.000000, rotZ=90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.073826 },
        { name = "hand.l", parent = "wrist.l", x=0.787008, y=1.106761, z=-0.000000, radius=0.016802, rotX=0.000000, rotY=-90.000000, rotZ=-90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "hand.r", parent = "wrist.r", x=-0.787008, y=1.106761, z=-0.000000, radius=0.016802, rotX=0.000000, rotY=90.000000, rotZ=90.000000, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "handslot.l", parent = "hand.l", x=0.883133, y=1.049261, z=-0.000000, radius=0.016802, rotX=90.000000, rotY=-0.000000, rotZ=179.999985, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
        { name = "handslot.r", parent = "hand.r", x=-0.883133, y=1.049261, z=-0.000000, radius=0.016802, rotX=90.000000, rotY=0.000000, rotZ=-179.999985, scaleX=1.000000, scaleY=1.000000, scaleZ=1.000000, length=0.112010 },
    },
}
local ARMATURE_TEMPLATES = {
    ARMATURE_NO_FINGERS_23,
    ARMATURE_NO_FINGERS_25,
    ARMATURE_2_CHAIN_FINGERS,
    ARMATURE_3_CHAIN_FINGERS,
    ARMATURE_STANDARD_SKELETON_65,
}

local function multiplyQuaternion(a,b)
    return {x=a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        y=a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        z=a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        w=a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}
end

local function normalizeQuaternion(q)
    local length=math.sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w)
    if length<=1e-8 then return nil end
    return {x=q.x/length,y=q.y/length,z=q.z/length,w=q.w/length}
end

local function eulerQuaternion(rotX,rotY,rotZ)
    local hx,hy,hz=math.rad(rotX or 0)*0.5,math.rad(rotY or 0)*0.5,
        math.rad(rotZ or 0)*0.5
    local qx={x=math.sin(hx),y=0,z=0,w=math.cos(hx)}
    local qy={x=0,y=math.sin(hy),z=0,w=math.cos(hy)}
    local qz={x=0,y=0,z=math.sin(hz),w=math.cos(hz)}
    return normalizeQuaternion(multiplyQuaternion(multiplyQuaternion(qx,qy),qz))
end

local function rotateRowVector(x,y,z,q)
    local xx,yy,zz=q.x*q.x,q.y*q.y,q.z*q.z
    local xy,xz,yz=q.x*q.y,q.x*q.z,q.y*q.z
    local xw,yw,zw=q.x*q.w,q.y*q.w,q.z*q.w
    return x*(1-2*(yy+zz))+y*(2*(xy-zw))+z*(2*(xz+yw)),
        x*(2*(xy+zw))+y*(1-2*(xx+zz))+z*(2*(yz-xw)),
        x*(2*(xz-yw))+y*(2*(yz+xw))+z*(1-2*(xx+yy))
end

local function calculateSkeletonBounds(template)
    if not template or type(template.bones)~='table' or #template.bones==0 then
        return nil
    end
    local bounds
    local function includePoint(x,y,z)
        if not bounds then
            bounds={minX=x,minY=y,minZ=z,maxX=x,maxY=y,maxZ=z}
            return
        end
        bounds.minX=math.min(bounds.minX,x); bounds.maxX=math.max(bounds.maxX,x)
        bounds.minY=math.min(bounds.minY,y); bounds.maxY=math.max(bounds.maxY,y)
        bounds.minZ=math.min(bounds.minZ,z); bounds.maxZ=math.max(bounds.maxZ,z)
    end
    for _,source in ipairs(template.bones) do
        if type(source.x)~='number' or type(source.y)~='number' or
                type(source.z)~='number' then return nil end
        local rotation=eulerQuaternion(source.rotX,source.rotY,source.rotZ)
        if not rotation then return nil end
        includePoint(source.x,source.y,source.z)
        local tailX,tailY,tailZ=rotateRowVector(0,source.length or 0,0,rotation)
        includePoint(source.x+tailX,source.y+tailY,source.z+tailZ)
    end
    bounds.height=bounds.maxY-bounds.minY
    return bounds
end

local function fit(template,target)
    local reference=calculateSkeletonBounds(template)
    if not target or not reference or type(target.minX)~='number' or
            type(target.minY)~='number' or type(target.minZ)~='number' or
            type(target.maxX)~='number' or type(target.maxY)~='number' or
            type(target.maxZ)~='number' then
        return nil,'invalid_template'
    end
    local referenceHeight=reference.height
    local targetHeight=target.maxY-target.minY
    if not referenceHeight or referenceHeight<=1e-6 or not targetHeight or targetHeight<=1e-6 then
        return nil,'invalid_bounds'
    end
    local scale=targetHeight/referenceHeight
    local referenceAnchor={x=(reference.minX+reference.maxX)*0.5,y=reference.minY,
        z=(reference.minZ+reference.maxZ)*0.5}
    local targetAnchor={x=(target.minX+target.maxX)*0.5,y=target.minY,
        z=(target.minZ+target.maxZ)*0.5}
    local fitted,byName={},{}
    for index,source in ipairs(template.bones) do
        if type(source.name)~='string' or source.name=='' or byName[source.name] then
            return nil,'invalid_template'
        end
        local parent=source.parent and byName[source.parent] or nil
        if source.parent and not parent then return nil,'invalid_template' end
        local globalPosition={x=targetAnchor.x+(source.x-referenceAnchor.x)*scale,
            y=targetAnchor.y+(source.y-referenceAnchor.y)*scale,
            z=targetAnchor.z+(source.z-referenceAnchor.z)*scale}
        local globalRotation=eulerQuaternion(source.rotX,source.rotY,source.rotZ)
        if not globalRotation then return nil,'invalid_template' end
        local translation={x=globalPosition.x,y=globalPosition.y,z=globalPosition.z}
        local rotation=globalRotation
        if parent then
            local inverse={x=-parent.globalRotation.x,y=-parent.globalRotation.y,
                z=-parent.globalRotation.z,w=parent.globalRotation.w}
            translation.x=globalPosition.x-parent.globalPosition.x
            translation.y=globalPosition.y-parent.globalPosition.y
            translation.z=globalPosition.z-parent.globalPosition.z
            translation.x,translation.y,translation.z=rotateRowVector(
                translation.x,translation.y,translation.z,inverse)
            rotation=normalizeQuaternion(multiplyQuaternion(globalRotation,inverse))
        end
        local bone={source=source,parentIndex=parent and parent.index or 0,index=index,
            translation=translation,rotation=rotation,globalPosition=globalPosition,
            globalRotation=globalRotation,radius=math.max(0,(source.radius or 0)*scale),
            length=math.max(0,(source.length or 0)*scale)}
        fitted[index]=bone
        byName[source.name]=bone
    end
    fitted.sourceBounds=reference
    fitted.height=targetHeight
    fitted.scale=scale
    return fitted,nil
end

local function apply(meshD,template,target)
    local fitted,fitError=fit(template,target)
    if not fitted then return false,fitError end
    local report=meshD:getSkeletonBindReport(false)
    if report and report.canonical and (report.boneCount or 0)>0 then
        meshD:removeAllSkeletalData()
    end
    local root=fitted[1]
    if root.parentIndex~=0 then return false,'invalid_template' end
    local source=root.source
    meshD:initializeSkeletalSkeleton(source.name,root.translation.x,root.translation.y,
        root.translation.z,root.radius,root.length,true)
    meshD:setSkeletalBoneBind(1,root.translation.x,root.translation.y,root.translation.z,
        root.rotation.x,root.rotation.y,root.rotation.z,root.rotation.w,
        source.scaleX or 1,source.scaleY or 1,source.scaleZ or 1,root.radius,root.length)
    for index=2,#fitted do
        local bone=fitted[index]
        local item=bone.source
        local created=meshD:addSkeletalBone(bone.parentIndex,item.name,
            bone.translation.x,bone.translation.y,bone.translation.z,bone.radius,bone.length,
            true,false)
        if created~=index then return false,'invalid_template' end
        meshD:setSkeletalBoneBind(index,bone.translation.x,bone.translation.y,bone.translation.z,
            bone.rotation.x,bone.rotation.y,bone.rotation.z,bone.rotation.w,
            item.scaleX or 1,item.scaleY or 1,item.scaleZ or 1,bone.radius,bone.length)
    end
    return true,#fitted,fitted.height
end

local ARMATURE_LABELS={}
for index,template in ipairs(ARMATURE_TEMPLATES) do
    ARMATURE_LABELS[index]=template.label
end

return {items=ARMATURE_TEMPLATES,labels=ARMATURE_LABELS,fit=fit,apply=apply}
