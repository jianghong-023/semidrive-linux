########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Strictly Confidential.
### ###########################################################################

# Mandatory bits
include ../common/window_systems/$(WINDOW_SYSTEM).mk
include ../common/apis/services.mk
include ../common/apis/egl.mk

# Optional bits
-include ../common/apis/vulkan.mk
-include ../common/apis/opengl.mk
-include ../common/apis/opengles1.mk
-include ../common/apis/opengles3.mk
-include ../common/apis/opencl.mk
-include ../common/apis/openrl.mk
-include ../common/apis/scripts.mk
#-include ../common/apis/rogue2d.mk

ifeq ($(PVR_REMOTE),1)
 -include ../common/apis/remote.mk
endif

ifeq ($(PVR_REMVIEW),1)
 -include ../common/apis/remview.mk
endif

# These must be included last
-include ../common/apis/unittests.mk
