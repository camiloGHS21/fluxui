package io.fluxui;

/** Validation rule (Zod-style). Build with Rule.string()/number()/boolean(). */
public final class Rule {
    enum Kind { STRING, NUMBER, BOOL }

    private final Kind kind;
    private boolean required = true;
    private Double min, max;
    private Integer minLen, maxLen;
    private boolean email;

    private Rule(Kind kind) {
        this.kind = kind;
    }

    public static Rule string() { return new Rule(Kind.STRING); }
    public static Rule number() { return new Rule(Kind.NUMBER); }
    public static Rule bool() { return new Rule(Kind.BOOL); }

    public Rule optional() { this.required = false; return this; }
    public Rule min(double v) { this.min = v; return this; }
    public Rule max(double v) { this.max = v; return this; }
    public Rule minLength(int v) { this.minLen = v; return this; }
    public Rule maxLength(int v) { this.maxLen = v; return this; }
    public Rule email() { this.email = true; return this; }

    String check(String field, String value) {
        if (value == null || value.isEmpty()) {
            return required ? field + " is required" : "";
        }
        switch (kind) {
            case NUMBER:
                double d;
                try {
                    d = Double.parseDouble(value);
                } catch (NumberFormatException e) {
                    return field + " must be a number";
                }
                if (min != null && d < min) return field + " is too small";
                if (max != null && d > max) return field + " is too large";
                break;
            case STRING:
                if (minLen != null && value.length() < minLen) return field + " is too short";
                if (maxLen != null && value.length() > maxLen) return field + " is too long";
                if (email && !value.contains("@")) return field + " must be a valid email";
                break;
            case BOOL:
                if (!value.equals("true") && !value.equals("false")) return field + " must be true/false";
                break;
        }
        return "";
    }
}
