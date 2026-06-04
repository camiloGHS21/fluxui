package io.fluxui;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Runtime validation schema (Zod-style).
 *
 * <pre>{@code
 *   Schema schema = new Schema()
 *       .field("email", Rule.string().email())
 *       .field("age",   Rule.number().min(18));
 *   Schema.Result res = schema.validate(Map.of("email", "a@b.com", "age", "20"));
 *   if (!res.ok) { ... res.errors ... }
 * }</pre>
 */
public final class Schema {
    public static final class Result {
        public final boolean ok;
        public final Map<String, String> errors;

        Result(boolean ok, Map<String, String> errors) {
            this.ok = ok;
            this.errors = errors;
        }

        public String first() {
            return errors.isEmpty() ? "" : errors.values().iterator().next();
        }
    }

    private final List<String[]> names = new ArrayList<>();
    private final List<Rule> rules = new ArrayList<>();

    public Schema field(String name, Rule rule) {
        names.add(new String[] {name});
        rules.add(rule);
        return this;
    }

    public Result validate(Map<String, String> data) {
        Map<String, String> errors = new LinkedHashMap<>();
        for (int i = 0; i < names.size(); i++) {
            String name = names.get(i)[0];
            String value = data.getOrDefault(name, "");
            String err = rules.get(i).check(name, value);
            if (!err.isEmpty()) {
                errors.put(name, err);
            }
        }
        return new Result(errors.isEmpty(), errors);
    }
}
