#!/bin/sh
# Build DialDeck.app: CoreBluetooth only works from a bundle whose Info.plist explains why the app
# uses Bluetooth, so the SwiftPM executable is wrapped into one and signed ad hoc.
#
# The binary is universal (Apple silicon and Intel), so the copy in mac/dist runs on any Mac.
#
#   scripts/build-app.sh            build into mac/build/DialDeck.app
#   scripts/build-app.sh --install  also copy it to /Applications, replacing an older copy
#   scripts/build-app.sh --dist     also copy it to mac/dist/DialDeck.app, the copy kept in git
set -eu

install=false
dist=false
for arg in "$@"; do
    case "$arg" in
    --install) install=true ;;
    --dist) dist=true ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

cd "$(dirname "$0")/.."
swift build -c release --arch arm64 --arch x86_64

app=build/DialDeck.app
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp .build/apple/Products/Release/DialDeck "$app/Contents/MacOS/DialDeck"

# Icon: render once at 1024 pt, scale into an iconset, pack it as .icns.
iconset=build/AppIcon.iconset
rm -rf "$iconset"
mkdir -p "$iconset"
swift scripts/make-icon.swift build/icon.png
for s in 16 32 128 256 512; do
    sips -z $s $s build/icon.png --out "$iconset/icon_${s}x${s}.png" >/dev/null
    sips -z $((s * 2)) $((s * 2)) build/icon.png --out "$iconset/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/AppIcon.icns"

# The commit the app was built from, marked dirty when there were uncommitted changes.
commit=$(git rev-parse --short HEAD)
if [ -n "$(git status --porcelain -- . ':!dist')" ]; then
    commit="$commit-dirty"
fi

cat > "$app/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>DialDeck</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>dev.kuroweb.dialdeck</string>
    <key>CFBundleName</key>
    <string>DialDeck</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1</string>
    <key>CFBundleVersion</key>
    <string>$commit</string>
    <key>LSUIElement</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>DialDeck reads and changes the button assignments of your DialDeck over Bluetooth.</string>
</dict>
</plist>
PLIST

codesign --force --sign - "$app"

if $dist; then
    rm -rf dist/DialDeck.app
    mkdir -p dist
    cp -R "$app" dist/
fi

if $install; then
    # A running copy keeps the old binary loaded.
    pkill -x DialDeck 2>/dev/null || true
    rm -rf /Applications/DialDeck.app
    cp -R "$app" /Applications/
    app=/Applications/DialDeck.app
fi
echo "$app"
