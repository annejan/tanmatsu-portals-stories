# Portals Stories: a Tanmatsu launcher plugin (README.md).
#
#   make            the plugin, build/portals-stories.plugin
#   make test       its fetching and card writing, on this computer
#   make install    onto the badge's card over BadgeLink, into /sd/plugins
#
# Needs ESP-IDF's RISC-V toolchain (IDF_PATH, IDF_TOOLS_PATH) and a
# tanmatsu-launcher checkout that has been built once (TANMATSU_PLUGIN_SDK,
# its tools/plugin-sdk; ../tanmatsu-launcher by default).

TANMATSU_PLUGIN_SDK ?= $(abspath ../tanmatsu-launcher/tools/plugin-sdk)
LAUNCHER            := $(abspath $(TANMATSU_PLUGIN_SDK)/../..)
BADGELINK           ?= $(abspath ../tanmatsu-launcher/badgelink/tools/badgelink.sh)
SLUG                := com.annejan.portals-stories
STORIES_URL         ?=

.PHONY: all test install clean
all:
	cmake -S . -B build -DTANMATSU_PLUGIN_SDK=$(TANMATSU_PLUGIN_SDK) $(if $(STORIES_URL),-DSTORIES_URL=$(STORIES_URL))
	cmake --build build

HOST_INC := -I$(LAUNCHER)/components/plugin-api/include \
	-I$(LAUNCHER)/managed_components/badgeteam__badge-elf-api/include \
	-I$(LAUNCHER)/managed_components/robotman2412__pax-gfx/core/include

test:
	mkdir -p build
	cc -O1 -Wall -Wextra -Wno-unused-parameter -DDLC_DIR='"card"' -DSITE='"site"' \
	  -DSTORIES_URL='"http://test/"' $(HOST_INC) tests/host.c -o build/host-test
	tests/run.sh build/host-test

install: all
	$(BADGELINK) fs mkdir /sd/plugins || true
	$(BADGELINK) fs mkdir /sd/plugins/$(SLUG) || true
	for f in plugin.json metadata.json icon16.png icon32.png icon64.png; do \
	  $(BADGELINK) fs upload /sd/plugins/$(SLUG)/$$f $(abspath $$f) || exit 1; done
	$(BADGELINK) fs upload /sd/plugins/$(SLUG)/portals-stories.plugin $(abspath build/portals-stories.plugin)

clean:
	rm -rf build
