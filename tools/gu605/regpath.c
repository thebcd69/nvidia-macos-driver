/* regpath <IOService path> ...: does the registry path resolve, and to what? (the check behind the NVRMDisplay@N fix)
 * clang -O1 -framework IOKit -framework CoreFoundation regpath.c -o regpath */
#include <IOKit/IOKitLib.h>
#include <stdio.h>
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        io_registry_entry_t e = IORegistryEntryFromPath(kIOMainPortDefault, argv[i]);
        if (!e) { printf("NOT FOUND: %s\n", argv[i]); continue; }
        uint64_t id = 0; io_name_t cls = ""; IORegistryEntryGetRegistryEntryID(e, &id); IOObjectGetClass(e, cls);
        printf("0x%llx %s <- %s\n", id, cls, argv[i]); IOObjectRelease(e);
    }
    return 0;
}
