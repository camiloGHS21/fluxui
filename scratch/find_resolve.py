with open(r"c:\Users\Administrator\Documents\prueba\fluxui\fluxui\src\css_parser.cpp", "r", encoding="utf-8", errors="ignore") as f:
    lines = f.readlines()

print("--- collectCandidateRules tail ---")
for idx in range(2539, 2580):
    if idx < len(lines):
        print(f"{idx+1}: {lines[idx].rstrip()}")
