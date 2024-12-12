#!/usr/bin/env python3
from pathlib import Path

Path("../KnShim/jni/andrleaf.h").write_text(Path("leaf.h").read_text().replace("printf(", '__android_log_print(ANDROID_LOG_INFO, "leaflib", '))
