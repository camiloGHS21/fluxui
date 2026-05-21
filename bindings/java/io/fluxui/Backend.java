package io.fluxui;

public enum Backend {
    AUTO(0),
    VULKAN(1),
    DIRECT3D12(2),
    METAL(3),
    SKIA(4),
    COMPATIBILITY(100);

    final int value;

    Backend(int value) {
        this.value = value;
    }
}
