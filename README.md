# Portals Stories

A plugin for the Tanmatsu (Konsool) launcher that puts the story packs for
[Portals](https://annejan.com/portals/) on the SD card over WiFi, from
[scene.rs/stories](https://scene.rs/stories/), and keeps them up to date.

Portals runs under Graceloader, which has no WiFi. The launcher has, so the
download happens here: pick **Portals Stories** in the launcher's Plugins
menu, and it lists the stories on scene.rs and what the card has of each.

| Key | |
|---|---|
| Up, Down | pick a story |
| Enter | put it on the card (new, update, or replace a copy put there by hand) |
| A | every new story and every update |
| Esc | back to the launcher |

Then start Portals: the stories are under Stories. A story goes to
`/sd/portals/dlc/<story>/`, where Portals reads it; the plugin leaves a
`.stories` file there, so the next update knows what changed. Saves are
Portals' own, in `/sd/portals/saves/`, and are not touched.

Needs a launcher with plugin API 3.1 (menu plugins that run when picked:
[tanmatsu-launcher#161](https://github.com/Nicolai-Electronics/tanmatsu-launcher/pull/161))
whose plugins can use HTTPS
([badge-elf-api#6](https://github.com/badgeteam/esp32-component-badge-elf-api/pull/6))
and draw the right way up
([badge-elf-api#7](https://github.com/badgeteam/esp32-component-badge-elf-api/pull/7)),
and an SD card. Until all three are in a launcher release, it runs on a
launcher built from those branches.

## What it fetches

`stories_site.py` in [tanmatsu-portals-grace](https://github.com/annejan/tanmatsu-portals-grace)
writes these next to the stories page:

- `index.txt`: a story a line, tab-separated: id, sha256, bytes, files,
  name, about. Lines starting `#` are comments.
- `packs/<id>/files.txt`: the story's files, `<bytes>\t<path>` a line.
- `packs/<id>/<path>`: each file as it goes on the card.

Every file of a story is fetched before any is written, and each must have
the size the list gives, so a dropped connection leaves the card as it was.

## Building

```
make TANMATSU_PLUGIN_SDK=/path/to/tanmatsu-launcher/tools/plugin-sdk
make test      # fetching and card writing, on this computer (tests/)
make install   # over BadgeLink, into /sd/plugins/com.annejan.portals-stories/
```

The SDK comes with a [tanmatsu-launcher](https://github.com/Nicolai-Electronics/tanmatsu-launcher)
checkout that has been built once; the build needs ESP-IDF's RISC-V
toolchain (`IDF_PATH`, `IDF_TOOLS_PATH`). `STORIES_URL=http://...` builds
one that fetches from somewhere else, for testing.

## License

MIT, see [LICENSE](LICENSE).
