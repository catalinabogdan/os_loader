# os_loader

This program implements an ELF executables loader, that treats in the 
loader.c file the error signals that might occur . When the signal differs from
SIGSEGV, it's not contained within the segments of the file or the faulty page
was already mapped, the default handler is called. Otherwise, after browsing
the segments array and finding the unmapped page by checking wether the data
array of that segment has the page size set at the correspondent index. If the
value is 0, the mapping can proceed using the MAP_ANONYMOUS flag, which sets
the mapped page with zeros. Consequently, the file's offset is set and it is
read in file the content of the page.
