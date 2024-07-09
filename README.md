# cdctap

**cdctap** reads SIMH-format tape images that come from various Control
Data Corporation (CDC) operating systems, primarily KRONOS and NOS,
that were written in "I" (internal) format.

**cdctap** has options similar to the UNIX **tar** program for viewing tape
contents and extracting items from the tape.

## Extraction: record types

**cdctap** can extract the following CDC record types:

- Text and CCL "PROC" records: extracted as ASCII text. Translation uses
64-character display code by default, but 63-character and 6/12 display
code are options.

- MODIFY OPL and OPLC records: latest revision extracted as ASCII text,
with or without sequence numbers. 6/12 display code translation is
not supported for these record types.

- PFDUMP records: extracted as SIMH-format tape images in CDC "I" format.
You can feed the resulting tape image through **cdctap** again to
view and extract the contents of the permanent file.
**cdctap** creates a separate directory per user index, e.g., the extracted
permanent files from user index 377776 are placed in a subdirectory
named "377776".

Tape images often contain multiple records with the same name, so
**cdctap** chooses a unique name for each extracted record. For example,
if "CIO.txt" already exists, **cdctap** will extract the next record
named "CIO" to "CIO.1.txt".

## Extraction: specification

You can specify the record names to be extracted using shell-type wildcards,
e.g. "cmrd\*". **cdctap** ignores upper/lower case when matching record names.

For PFDUMP records, you can limit extraction to a particular user index
via the syntax "*ui*/*recname*". Also, **cdctap** recognizes a small set
of built-in user number to user index translations, e.g., "SYSTEMX" for
user index 377777.

## References

- SIMH tape format: http://www.bitsavers.org/pdf/simh/simh_magtape.pdf

- Control Data Corporation NOS: http://www.bitsavers.org/pdf/cdc/cyber/
