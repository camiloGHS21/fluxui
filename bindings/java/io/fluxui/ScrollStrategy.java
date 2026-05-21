package io.fluxui;

public enum ScrollStrategy {
    START(0),
    CENTER(1),
    END(2),
    NEAREST(3);

    final int value;

    ScrollStrategy(int value) {
        this.value = value;
    }
}
