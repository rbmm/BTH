
public codesec64_kb_begin, codesec64_kb_end, cert_begin, cert_end, inf_begin, inf_end, cat_begin, cat_end

CONST segment

codesec64_kb_begin:
INCLUDE <..\x64\release\bthcli.asm>
codesec64_kb_end:

cert_begin:
INCLUDE <..\ibscr\testsigncert.asm>
cert_end:

inf_begin:
INCLUDE <..\pkg\bthcli\inf.asm>
inf_end:

cat_begin:
INCLUDE <..\pkg\bthcli\cat.asm>
cat_end:

CONST ends

end