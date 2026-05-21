package io.fluxui;

public final class Modifiers {
    public static final int NONE = 0;
    public static final int SHIFT = 1 << 0;
    public static final int CTRL = 1 << 1;
    public static final int ALT = 1 << 2;
    public static final int GUI = 1 << 3;

    private Modifiers() {
    }
}
