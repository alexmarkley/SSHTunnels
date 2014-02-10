include theos/makefiles/common.mk

TOOL_NAME = SSHTunnels
SSHTunnels_FILES = main.c log.c util.c

include $(THEOS_MAKE_PATH)/tool.mk
