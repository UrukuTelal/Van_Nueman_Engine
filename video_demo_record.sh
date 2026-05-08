#!/bin/bash
# Video demo recording script for Van Nueman
# Requires: OBS Studio, ffmpeg, or similar screen capture

RESOLUTION="1920x1080"
FRAMERATE="60"
DURATION="60"  # seconds per scenario
OUTPUT_DIR="C:/Projects/Van_Nueman/docs/videos"

mkdir -p "$OUTPUT_DIR"

echo "=== Van Nueman Video Demo Recording ==="
echo "Resolution: $RESOLUTION @ $FRAMERATE FPS"
echo "Output: $OUTPUT_DIR"
echo ""

echo "Starting demo in 5 seconds..."
sleep 5

# Scenario 1: Pillar HUD Demo (30 seconds)
echo "Recording: Pillar HUD Demo (30s)..."
# Press 'P' to toggle pillar overlay
# Show each pillar with green/yellow/red colors
ffmpeg -f gdigrab -framerate $FRAMERATE -video_size $RESOLUTION -i desktop \
       -t 30 -c:v libx264 -preset fast -crf 23 \
       "$OUTPUT_DIR/pillar_hud_demo.mp4" 2>/dev/null &

cd C:/Projects/Van_Nueman/build/Release
timeout 30 ./van-nueman-game.exe 2>&1 | head -20
kill %1 2>/dev/null

echo "Demo complete. Videos saved to: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"/*.mp4 2>/dev/null
