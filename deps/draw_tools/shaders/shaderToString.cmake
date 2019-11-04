set(source "")
file(STRINGS ${IN} lines)
foreach (line ${lines})
    set(source "${source}\"${line}\\n\"\n")
endforeach()
file(WRITE ${OUT} "static GLchar ${VAR}[] = {\n"
     "    ${source}"
     "};\n")
