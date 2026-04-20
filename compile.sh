$VULKAN_SDK/bin/slangc ./src/shaders/shader.slang -target spirv -profile spirv_1_4 \
    -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain \
    -o ./src/shaders/slang.spv

mkdir -p build/shaders
rm -f build/shaders/slang.spv
cp ./src/shaders/slang.spv build/shaders/slang.spv

# Copy textures folder
mkdir -p build/textures
cp -r ./src/textures/* build/textures/
