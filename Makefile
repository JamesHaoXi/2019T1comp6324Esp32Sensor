#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := ble_azureIoT_gateway

EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../../esp-azure

include $(IDF_PATH)/make/project.mk

