#!/usr/bin/env bash
set -euo pipefail

DOCKER_IMAGE=dase-build

echo "==> [1/3] Building Docker image..."
docker build -f Dockerfile.build -t "$DOCKER_IMAGE" . -q

echo "==> [2/3] Compiling UEFI app..."
docker run --rm -v "$(pwd):/work" "$DOCKER_IMAGE" make

echo "==> [3/3] Creating boot image..."
docker run --rm -v "$(pwd):/work" "$DOCKER_IMAGE" bash scripts/mkimage.sh

echo ""
echo "==> Build complete: build/dase.img"
echo ""