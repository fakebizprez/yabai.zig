<!-- Please be careful editing the below HTML, as GitHub is quite finicky with anything that looks like an HTML tag in GitHub Flavored Markdown. -->
<p align="center">
  <img width="75%" src="assets/banner/banner.svg" alt="yabai-zig banner">
</p>
<p align="center">
  <b>Tiling window management for the Mac.</b>
</p>
<p align="center">
  <a href="LICENSE">
    <img src="https://img.shields.io/github/license/fakebizprez/yabai.zig.svg?color=green" alt="License Badge">
  </a>
  <a href="doc/yabai-zig.asciidoc">
    <img src="https://img.shields.io/badge/view-documentation-green.svg" alt="Documentation Badge">
  </a>
  <a href="CHANGELOG.md">
    <img src="https://img.shields.io/badge/view-changelog-green.svg" alt="Changelog Badge">
  </a>
</p>

yabai-zig is the [&nearr;&nbsp;yabai][gh-yabai] tiling window manager for macOS with two changes: it runs on macOS 27 (Golden Gate), including Space switching with System Integrity Protection left on, and its logic is being moved from C into Zig one tested piece at a time.
It is a fork of yabai 7.1.25 by Åsmund Vikane, and every command, setting and query works the same way.

## Status

- **Zig port:** 107 functions have Zig versions, tested against the original C so they produce the same results bit for bit; 36 of them are what the daemon actually runs today. The rest of the program is still C and Objective-C. The [routing map](doc/zig-baseline.md#routing-map-2026-10-22-107-exports-8-objects-106-tests) lists what runs where.
- **macOS 27:** backported from [&nearr;&nbsp;Christian-SC26/yabai][gh-christian]. Tested on Apple Silicon, macOS 27.0.1. Switching across several Spaces at once flashes the Spaces in between, and moving or reordering whole Spaces is not supported. Details are in the [verification record](doc/zig-release.md).
- **Releases:** none yet, and there is no CI. Build from source.
- **Side by side with yabai:** the binary, socket, config file, environment variables, scripting addition and service label are all named `yabai-zig`, so it does not collide with an installed yabai. Do not run both at once; they would fight over the same windows.

Report bugs in this fork [here](https://github.com/fakebizprez/yabai.zig/issues), not upstream.

## What it does

yabai-zig is a window management utility that works as an extension to the built-in window manager of macOS.
It lets you control your windows, spaces and displays from the command line, and you can bind those commands to keyboard shortcuts with [&nearr;&nbsp;skhd][gh-skhd] or similar tools.

Its main job is tiling: it lays out your windows automatically using binary space partitioning, so you can focus on their content.
It also offers focus-follows-mouse, disabling Space-switch animations, creating more than 16 Spaces, and much more.

## Build and run

You need the Xcode command line tools and Zig 0.17.0 (the version pinned in `zig/ZIG_VERSION`; the build refuses any other).

```sh
make install                                  # optimized universal build at bin/yabai-zig (it does not install anything)
cp bin/yabai-zig /usr/local/bin/              # or anywhere on your PATH
yabai-zig --install-service && yabai-zig --start-service
```

macOS asks for Accessibility permission on first launch; restart the service after granting it.
To keep that permission across rebuilds, sign the binary with a stable identity: create a code-signing certificate named `yabai-zig-cert` in Keychain Access, then run `make sign`.

Run the headless test suite with `cd tests && make`.

## Configuration

- The config file is `~/.config/yabai-zig/yabai-zigrc`. Start from the samples in the [examples][yabai-examples] directory.
- The [documentation][yabai-docs] covers every command and setting.
- The [&nearr;&nbsp;upstream yabai wiki][yabai-wiki] is still a useful guide; substitute `yabai-zig` for `yabai` in its commands and paths.

## Requirements and caveats

Please read the requirements below carefully, and make sure you meet all of them before filing an issue.

|Requirement|Note|
|-:|:-|
|Operating&nbsp;System&nbsp;Intel x86-64|Big Sur 11.0.0+, Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, and Tahoe 26.0+ is supported.|
|Operating&nbsp;System&nbsp;Apple Silicon|Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, Tahoe 26.0+, and Golden Gate 27.0+ (see [Status](#status)) is supported.|
|Accessibility&nbsp;API|yabai-zig must be given permission to utilize the Accessibility API and will request access upon launch. The application must be restarted after access has been granted.|
|Screen Recording|yabai-zig must be given Screen Recording permission if and only if you want to enable window animations, and will request access when necessary. The application must be restarted after access has been granted.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x, 27.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|

Please also take note of the following caveats.

|Caveat|Note|
|-:|:-|
|System&nbsp;Integrity&nbsp;Protection (Optional)|System Integrity Protection can be (partially) disabled for yabai-zig to inject a scripting addition into Dock.app for controlling windows with functions that require elevated privileges. This enables control of the window server, which is the sole owner of all window connections, and enables additional features of yabai-zig.|
|Code&nbsp;Signing|When building from source (or installing from HEAD), it is necessary to codesign the binary so it retains its accessibility and automation privileges when updated or rebuilt.|
|Finder&nbsp;Desktop|Some people disable the Finder Desktop window using an undocumented defaults write command. This breaks focusing of empty spaces and should be avoided when using yabai-zig. To re-activate the Finder Desktop, run: "defaults write com.apple.finder CreateDesktop -bool true".|
|NSDocument-based&nbsp;Applications|Windows that utilize native macOS tabs such as Terminal and Finder, [do not behave correctly when creating tabs](https://github.com/asmvik/yabai/issues/68). Avoid creating tabs in these applications, consider alternatives that do not use NSDocument's tab system, or make these windows float using rules.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x, 27.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x, 27.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Show Items On Desktop" should be enabled for display and space focus commands to work reliably in multi-display configurations.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x, 27.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Click wallpaper to reveal Desktop" should be set to "Only in Stage Manager" for display and space focus commands to work reliably.|

## License and Attribution

yabai-zig is licensed under the [&nearr;&nbsp;MIT&nbsp;License][yabai-license], a short and simple permissive license with conditions only requiring preservation of copyright and license notices.
Licensed works, modifications, and larger works may be distributed under different terms and without source code.

yabai was created by Åsmund Vikane ([&nearr;&nbsp;asmvik/yabai][gh-yabai]); this fork keeps the original copyright notice alongside its own. The macOS 27 support comes from [&nearr;&nbsp;Christian-SC26/yabai][gh-christian].

Thanks to [@fools-mate][gh-fools-mate] for creating the original yabai logo and banner, which the yabai-zig banner is adapted from, and making them available for free.

Thanks to [@dominiklohmann][gh-dominiklohmann] for contributing great documentation, support, and more, for free.

## Disclaimer

Use at your own discretion.
I take no responsibility if anything should happen to your machine while trying to install, test or otherwise use this software in any form.
You acknowledge that you understand the potential risk that may come from disabling [&nearr;&nbsp;System&nbsp;Integrity&nbsp;Protection][external-about-sip] on your system, and I make no recommendation as to whether you should or should not disable System Integrity Protection.

<!-- Project internal links -->
[yabai-license]: LICENSE
[yabai-examples]: examples/
[yabai-wiki]: https://github.com/asmvik/yabai/wiki
[yabai-docs]: doc/yabai-zig.asciidoc

<!-- Links to other GitHub projects/users -->
[gh-yabai]: https://github.com/asmvik/yabai
[gh-christian]: https://github.com/Christian-SC26/yabai
[gh-skhd]: https://github.com/asmvik/skhd
[gh-fools-mate]: https://github.com/fools-mate
[gh-dominiklohmann]: https://github.com/dominiklohmann

<!-- External links -->
[external-about-sip]: https://support.apple.com/en-us/HT204899
