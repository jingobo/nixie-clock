# Project specific
PROJECT_SOURCE_DIRS = source source/web source/svc ../common/source
# IDF Build System
COMPONENT_SRCDIRS = $(PROJECT_SOURCE_DIRS)
COMPONENT_ADD_INCLUDEDIRS = $(PROJECT_SOURCE_DIRS)
CXXFLAGS += -Wno-reorder -Wno-type-limits