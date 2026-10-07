#!/bin/bash
# Build script for Java/GraalVM applications
# This script builds the Java file manager using GraalVM native-image

set -e

MYOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$MYOS_ROOT/build"
JAVA_SRC_DIR="$MYOS_ROOT/user/java"
FILES_DIR="$JAVA_SRC_DIR/files"
TOOLKIT_DIR="$JAVA_SRC_DIR/toolkit"

# Check for GraalVM
if ! command -v native-image &> /dev/null; then
    if [ -n "$GRAALVM_HOME" ] && [ -f "$GRAALVM_HOME/bin/native-image" ]; then
        export PATH="$GRAALVM_HOME/bin:$PATH"
    elif [ -f "/usr/lib/graalvm/bin/native-image" ]; then
        export PATH="/usr/lib/graalvm/bin:$PATH"
    elif [ -f "/opt/graalvm/bin/native-image" ]; then
        export PATH="/opt/graalvm/bin:$PATH"
    else
        echo "Error: GraalVM native-image not found!"
        echo "Please install GraalVM and set GRAALVM_HOME or add native-image to PATH"
        echo "Visit: https://www.graalvm.org/downloads/"
        exit 1
    fi
fi

echo "Using native-image: $(which native-image)"
echo "GraalVM version: $(native-image --version)"

# Create build directory
mkdir -p "$BUILD_DIR/java"

# Compile Java sources
echo "Compiling Java sources..."
javac -d "$BUILD_DIR/java/classes" \
    --release 17 \
    -sourcepath "$JAVA_SRC_DIR" \
    "$TOOLKIT_DIR"/*.java \
    "$FILES_DIR"/*.java

# Create JAR
echo "Creating JAR..."
cd "$BUILD_DIR/java/classes"
jar cfe "$BUILD_DIR/java/files.jar" files.Main \
    toolkit/*.class \
    files/*.class \
    META-INF/

# Build native image
echo "Building native image..."
cd "$MYOS_ROOT"
native-image \
    --static \
    --no-fallback \
    --no-server \
    -H:EnableURLProtocols=http,https \
    -H:+ReportUnsupportedElementsAtRuntime \
    -H:ReflectionConfigurationFiles="$FILES_DIR/META-INF/native-image/files/reflect-config.json" \
    -H:JNIConfigurationFiles="$FILES_DIR/META-INF/native-image/files/jni-config.json" \
    -H:DynamicProxyConfigurationFiles="$FILES_DIR/META-INF/native-image/files/proxy-config.json" \
    -H:Name=files \
    -H:Class=files.Main \
    -cp "$BUILD_DIR/java/classes" \
    -o "$BUILD_DIR/user/files"

# Also build the toolkit library
echo "Building toolkit library..."
native-image \
    --static \
    --no-fallback \
    --no-server \
    -H:Name=myos-toolkit \
    -cp "$BUILD_DIR/java/classes" \
    -o "$BUILD_DIR/user/myos-toolkit" \
    --shared

echo "Build complete!"
echo "Executable: $BUILD_DIR/user/files"
echo "Shared library: $BUILD_DIR/user/myos-toolkit.so"

# Copy to user bin directory for embedding
mkdir -p "$BUILD_DIR/user"
cp "$BUILD_DIR/user/files" "$BUILD_DIR/user/files.elf"