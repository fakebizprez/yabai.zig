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

yabai-zig is a fork of [&nearr;&nbsp;asmvik/yabai][gh-yabai] by Åsmund Vikane (7.1.25, commit `dd84572`) that is being ported to Zig piece by piece, with macOS 27 support backported from [&nearr;&nbsp;Christian-SC26/yabai][gh-christian].
The C/Objective-C daemon is still the program; pure logic moves into Zig objects that the C code calls through a C ABI.
It is renamed throughout (binary, socket, config file, scripting addition, service label), so it can be installed next to upstream yabai.
There are no releases or CI yet; build it from source.
Report bugs in this fork here, not upstream.

## About

yabai-zig is a window management utility that is designed to work as an extension to the built-in window manager of macOS.
yabai-zig allows you to control your windows, spaces and displays freely using an intuitive command line interface and optionally set user-defined keyboard shortcuts using [&nearr;&nbsp;skhd][gh-skhd] and other third-party software.

The primary function of yabai-zig is tiling window management; automatically modifying your window layout using a binary space partitioning algorithm to allow you to focus on the content of your windows without distractions.
Additional features of yabai-zig include focus-follows-mouse, disabling animations for switching spaces, creating spaces past the limit of 16 spaces, and much more.

## Building

Requires Xcode command line tools and Zig 0.17.0 (the version pinned in `zig/ZIG_VERSION`).

```sh
make install          # optimized universal build at bin/yabai-zig
cd tests && make      # headless test suite
make sign             # optional: sign with a "yabai-zig-cert" identity (see scripts/codesign)
```

`make install` only builds; copy `bin/yabai-zig` somewhere on your `PATH`, then run `yabai-zig --install-service` and `yabai-zig --start-service`.

## Configuration

- Sample configuration files can be found in the [examples][yabai-examples] directory. Refer to the [documentation][yabai-docs] for every command and setting.
- The [&nearr;&nbsp;upstream yabai wiki][yabai-wiki] is still a useful reference; substitute `yabai-zig` for `yabai` in its commands and paths.
- Keyboard shortcuts can be defined with [&nearr;&nbsp;skhd][gh-skhd] or any other suitable software you may prefer.

## Requirements and Caveats

Please read the below requirements carefully.
Make sure you fulfil all of them before filing an issue.

On macOS 27, universal builds and headless tests pass on 27.0.1.
Switching across several Spaces at once still flashes, and moving or reordering entire Spaces is unsupported.
See the [verification record](doc/zig-release.md) for the tested scope.

|Requirement|Note|
|-:|:-|
|Operating&nbsp;System&nbsp;Intel x86-64|Big Sur 11.0.0+, Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, and Tahoe 26.0+ is supported.|
|Operating&nbsp;System&nbsp;Apple Silicon|Monterey 12.0.0+, Ventura 13.0.0+, Sonoma 14.0.0+, Sequoia 15.0+, and Tahoe 26.0+ is supported.|
|Accessibility&nbsp;API|yabai-zig must be given permission to utilize the Accessibility API and will request access upon launch. The application must be restarted after access has been granted.|
|Screen Recording|yabai-zig must be given Screen Recording permission if and only if you want to enable window animations, and will request access when necessary. The application must be restarted after access has been granted.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Displays have separate Spaces" must be enabled.|

Please also take note of the following caveats.

|Caveat|Note|
|-:|:-|
|System&nbsp;Integrity&nbsp;Protection (Optional)|System Integrity Protection can be (partially) disabled for yabai-zig to inject a scripting addition into Dock.app for controlling windows with functions that require elevated privileges. This enables control of the window server, which is the sole owner of all window connections, and enables additional features of yabai-zig.|
|Code&nbsp;Signing|When building from source (or installing from HEAD), it is necessary to codesign the binary so it retains its accessibility and automation privileges when updated or rebuilt.|
|Finder&nbsp;Desktop|Some people disable the Finder Desktop window using an undocumented defaults write command. This breaks focusing of empty spaces and should be avoided when using yabai-zig. To re-activate the Finder Desktop, run: "defaults write com.apple.finder CreateDesktop -bool true".|
|NSDocument-based&nbsp;Applications|Windows that utilize native macOS tabs such as Terminal and Finder, [do not behave correctly when creating tabs](https://github.com/asmvik/yabai/issues/68). Avoid creating tabs in these applications, consider alternatives that do not use NSDocument's tab system, or make these windows float using rules.|
|System&nbsp;Preferences&nbsp;(macOS 11.x, 12.x)|In the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 13.x, 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Mission Control pane, the setting "Automatically rearrange Spaces based on most recent use" should be disabled for commands that rely on the ordering of spaces to work reliably.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Show Items On Desktop" should be enabled for display and space focus commands to work reliably in multi-display configurations.|
|System&nbsp;Settings&nbsp;(macOS 14.x, 15.x, 26.x)|In the Desktop & Dock tab, inside the Desktop & Stage Manager pane, the setting "Click wallpaper to reveal Desktop" should be set to "Only in Stage Manager" for display and space focus commands to work reliably.|

## License and Attribution

yabai-zig is licensed under the [&nearr;&nbsp;MIT&nbsp;License][yabai-license], a short and simple permissive license with conditions only requiring preservation of copyright and license notices.
Licensed works, modifications, and larger works may be distributed under different terms and without source code.

yabai was created by Åsmund Vikane ([&nearr;&nbsp;asmvik/yabai][gh-yabai]); this fork keeps the original copyright notice alongside its own.

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
