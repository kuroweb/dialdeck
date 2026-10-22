#!/bin/sh
# Build DialDeck.app: CoreBluetooth only works from a bundle whose Info.plist explains why the app
# uses Bluetooth, so the SwiftPM executable is wrapped into one and signed ad hoc.
#
#   scripts/build-app.sh            build into mac/build/DialDeck.app
#   scripts/build-app.sh --install  also copy it to /Applications, replacing an older copy
set -eu

install=false
if [ "${1:-}" = "--install" ]; then
    install=true
fi

cd "$(dirname "$0")/.."
swift build -c release

app=build/DialDeck.app
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp .build/release/DialDeck "$app/Contents/MacOS/DialDeck"

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

cat > "$app/Contents/Info.plist" <<'PLIST'
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
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>DialDeck reads and changes the button assignments of your DialDeck over Bluetooth.</string>
</dict>
</plist>
PLIST

codesign --force --sign - "$app"

if $install; then
    # A running copy keeps the old binary loaded.
    pkill -x DialDeck 2>/dev/null || true
    rm -rf /Applications/DialDeck.app
    cp -R "$app" /Applications/
    app=/Applications/DialDeck.app
fi
echo "$app"
