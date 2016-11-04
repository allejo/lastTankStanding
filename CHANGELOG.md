# Changelog

## 1.1.0

**New**

- Add sanity checks at plug-in load to help server owners with configuration
- Matches can now be recorded with BZFS' `-recdir` option
- A scoreboard is now displayed at the end of each LTS match

**Fixes**

- Corrected message regarding when players can join

**Changes**

- Plug-in is now licensed under MIT
- Include updates to latest bzToolkit
- The `/end` command has been changed to `/gameover`

## 1.0.3

**New**

- Added plug-in versioning
- The '/start' command now accepts a parameter for a custom countdown time

**Changes**

- Updated pointers used throughout the plug-in
- Used BZFS API to automatically add players as an observer if a match is progress

**Fixes**

- Plug-in now respects -setforced values for _lts* BZDB settings

## 1.0.2

**Changes**

- Use more recent build of bzToolkit

**Fixed**

- Fixed 17.1 year elimination round bug
- Fixed default permission for the /end command

## 1.0.1

**New**

- New Supports optional configuration file to specify permissions for the /start and /end commands without modifying the source code of the plugin.
- New Automatically kick paused or idling players trying to save their score
- New Use of updated bzToolkit version

**Fixed**

- Fixed backwards countdown when announcing time until next elimination

**Changes**

- Requirement of rewritten BZFlag API function
- Use of more C++11 features

## 1.0.0

Initial release