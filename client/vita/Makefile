TOPDIR ?= $(CURDIR)

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing header files
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
#---------------------------------------------------------------------------------
TARGET		:=	plybck-vita
BUILD		:=	build
SOURCES	:=	. ../lib
INCLUDES	:=	include ../lib
APP_TITLE   := PlayBack Vita
APP_TITLEID := PLYBCKVIT

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS	:=	-g -Wall -Wextra -flto -Os \
			-fomit-frame-pointer -ffunction-sections  -Wl,-q

CFLAGS	+=	$(INCLUDE)

LDFLAGS	=	-Wl,-Map,$(notdir $*.map) -flto -Wl,-q

LIBS	:= -lSceDisplay_stub -lSceSysmodule_stub
#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
CC := arm-vita-eabi-gcc
STRIP := arm-vita-eabi-strip

ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

export LD	:=	$(CC)

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).vpk


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------


$(OUTPUT).vpk: eboot.bin param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin \
		--add $(TOPDIR)/sce_sys/icon0.png=sce_sys/icon0.png \
		--add $(TOPDIR)/sce_sys/livearea/contents/bg.png=sce_sys/livearea/contents/bg.png \
		--add $(TOPDIR)/sce_sys/livearea/contents/startup.png=sce_sys/livearea/contents/startup.png \
		--add $(TOPDIR)/sce_sys/livearea/contents/template.xml=sce_sys/livearea/contents/template.xml \
	$(OUTPUT).vpk

eboot.bin: $(TARGET).velf
	vita-make-fself -c $(TARGET).velf eboot.bin

param.sfo:
	vita-mksfoex -s TITLE_ID="$(APP_TITLEID)" "$(APP_TITLE)" param.sfo

$(TARGET).velf: $(TARGET).elf
	$(STRIP) -g $<
	vita-elf-create $< $@

$(TARGET).elf	:	$(OFILES)

%.elf:
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES)  $(LIBS) -o $@

%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
