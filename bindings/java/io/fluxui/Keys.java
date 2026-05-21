package io.fluxui;

public final class Keys {
    public static final int ESCAPE = 0x1B;
    public static final int ENTER = 0x0D;
    public static final int SPACE = 0x20;
    public static final int LEFT = 0x25;
    public static final int UP = 0x26;
    public static final int RIGHT = 0x27;
    public static final int DOWN = 0x28;

    private Keys() {
    }

    public static int letter(char value) {
        return Character.toUpperCase(value);
    }
}
