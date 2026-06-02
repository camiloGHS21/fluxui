with open(r"c:\Users\Administrator\Documents\prueba\fluxui\fluxui\src\widgets.cpp", "r", encoding="utf-8", errors="ignore") as f:
    lines = f.readlines()

print("--- renderChildren in widgets.cpp ---")
for idx, line in enumerate(lines):
    if "renderChildren" in line or "Widget::render" in line:
        if "void" in line:
            print(f"Line {idx+1}: {line.strip()}")
            for j in range(1, 20):
                print(f"  {idx+j+1}: {lines[idx+j].rstrip()}")
