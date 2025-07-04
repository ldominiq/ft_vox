#!/bin/bash
set -e

# Install glad if not already installed
pip show glad > /dev/null 2>&1 || pip install glad

# Generate GLAD for OpenGL 3.3 Core profile
glad --generator=c --spec=gl --api="gl=3.3" --profile=core --out-path=glad

